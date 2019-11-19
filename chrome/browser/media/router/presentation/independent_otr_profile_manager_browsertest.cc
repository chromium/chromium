// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/media/router/presentation/independent_otr_profile_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

class ProfileDestructionWatcher : public ProfileObserver {
 public:
  ProfileDestructionWatcher() = default;
  ~ProfileDestructionWatcher() override = default;

  void Watch(Profile* profile) { observed_profiles_.Add(profile); }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    destroyed_ = true;
    observed_profiles_.Remove(profile);
  }

  bool destroyed() const { return destroyed_; }

 private:
  bool destroyed_ = false;
  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileDestructionWatcher);
};

// Waits for |browser| to be removed from BrowserList and then calls |callback|.
// This is used to ensure that the Browser object and its window are destroyed
// after a call to BrowserWindow::Close, since base::RunLoop::RunUntilIdle
// doesn't ensure this on Mac.
class BrowserRemovedWaiter final : public BrowserListObserver {
 public:
  BrowserRemovedWaiter(Browser* browser, base::OnceClosure callback)
      : browser_(browser), callback_(std::move(callback)) {
    BrowserList::AddObserver(this);
  }
  ~BrowserRemovedWaiter() override = default;

  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      BrowserList::RemoveObserver(this);
      std::move(callback_).Run();
    }
  }

 private:
  Browser* browser_;
  base::OnceClosure callback_;
};

void OriginalProfileNeverDestroyed(Profile* profile) {
  FAIL()
      << "Original profile unexpectedly destroyed before dependent OTR profile";
}

// This class acts as an owner of an OTRProfileRegistration and deletes the
// registration when it is notified that the original profile is being
// destroyed.  This is the minimum behavior expected by owners of
// OTRProfileRegistration.
class RegistrationOwner {
 public:
  // |profile| is an original Profile from which we are creating a registered
  // OTR profile.  |this| will own the resulting OTR profile registration.
  RegistrationOwner(IndependentOTRProfileManager* manager, Profile* profile)
      : otr_profile_registration_(manager->CreateFromOriginalProfile(
            profile,
            base::BindOnce(&RegistrationOwner::OriginalProfileDestroyed,
                           base::Unretained(this)))) {}

  Profile* profile() const { return otr_profile_registration_->profile(); }

 private:
  void OriginalProfileDestroyed(Profile* profile) {
    DCHECK(profile == otr_profile_registration_->profile());
    otr_profile_registration_.reset();
  }

  std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>
      otr_profile_registration_;
};

}  // namespace

class IndependentOTRProfileManagerTest : public InProcessBrowserTest {
 protected:
  void EnableProfileHelperTestSettings() {
    chromeos::ProfileHelper::Get()->SetProfileToUserForTestingEnabled(true);
    chromeos::ProfileHelper::Get()->SetAlwaysReturnPrimaryUserForTesting(true);
  }

  IndependentOTRProfileManager* manager_ =
      IndependentOTRProfileManager::GetInstance();
};

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest, CreateAndDestroy) {
  ProfileDestructionWatcher watcher;
  {
    auto profile_registration = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile = profile_registration->profile();

    ASSERT_NE(browser()->profile(), otr_profile);
    EXPECT_NE(browser()->profile()->GetOffTheRecordProfile(), otr_profile);
    EXPECT_TRUE(otr_profile->IsOffTheRecord());

    watcher.Watch(otr_profile);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       DeleteWaitsForLastBrowser) {
  ProfileDestructionWatcher watcher;
  Profile* otr_profile = nullptr;
  Browser* otr_browser1 = nullptr;
  Browser* otr_browser2 = nullptr;
  {
    auto profile_registration = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    otr_profile = profile_registration->profile();

    otr_browser1 = CreateBrowser(otr_profile);
    otr_browser2 = CreateBrowser(otr_profile);
    ASSERT_NE(otr_browser1, otr_browser2);
  }

  base::RunLoop run_loop1;
  BrowserRemovedWaiter removed_waiter1(otr_browser1,
                                       run_loop1.QuitWhenIdleClosure());
  otr_browser1->window()->Close();
  run_loop1.Run();
  ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser1));
  ASSERT_TRUE(base::Contains(*BrowserList::GetInstance(), otr_browser2));

  watcher.Watch(otr_profile);
  base::RunLoop run_loop2;
  BrowserRemovedWaiter removed_waiter2(otr_browser2,
                                       run_loop2.QuitWhenIdleClosure());
  otr_browser2->window()->Close();
  run_loop2.Run();
  ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser2));
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       DeleteImmediatelyWhenBrowsersAlreadyClosed) {
  ProfileDestructionWatcher watcher;
  {
    auto profile_registration = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile = profile_registration->profile();

    auto* otr_browser1 = CreateBrowser(otr_profile);
    auto* otr_browser2 = CreateBrowser(otr_profile);
    ASSERT_NE(otr_browser1, otr_browser2);

    base::RunLoop run_loop1;
    BrowserRemovedWaiter removed_waiter1(otr_browser1,
                                         run_loop1.QuitWhenIdleClosure());
    base::RunLoop run_loop2;
    BrowserRemovedWaiter removed_waiter2(otr_browser2,
                                         run_loop2.QuitWhenIdleClosure());
    otr_browser1->window()->Close();
    otr_browser2->window()->Close();
    run_loop1.Run();
    run_loop2.Run();
    ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser1));
    ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser2));

    watcher.Watch(otr_profile);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       CreateTwoFromSameProfile) {
  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;
  {
    auto profile_registration1 = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile1 = profile_registration1->profile();

    auto profile_registration2 = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile2 = profile_registration2->profile();

    ASSERT_NE(otr_profile1, otr_profile2);

    watcher1.Watch(otr_profile1);
    watcher2.Watch(otr_profile2);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher1.destroyed());
  EXPECT_TRUE(watcher2.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       OriginalProfileDestroyedFirst) {
  ProfileDestructionWatcher watcher;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
#if defined(OS_CHROMEOS)
  EnableProfileHelperTestSettings();
#endif
  auto original_profile = Profile::CreateProfile(
      temp_dir.GetPath(), nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  ASSERT_TRUE(original_profile);
  auto profile_owner = RegistrationOwner(manager_, original_profile.get());
  auto* otr_profile = profile_owner.profile();

  ASSERT_NE(original_profile.get(), otr_profile);
  EXPECT_NE(original_profile->GetOffTheRecordProfile(), otr_profile);

  watcher.Watch(otr_profile);
  // Run tasks to ensure that Mojo connections are created before the profile is
  // destroyed.
  base::RunLoop().RunUntilIdle();
  original_profile.reset();
  // |original_profile| being destroyed should trigger the dependent OTR
  // profile to be destroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       OriginalProfileDestroyedFirstTwoOTR) {
  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
#if defined(OS_CHROMEOS)
  EnableProfileHelperTestSettings();
#endif
  auto original_profile = Profile::CreateProfile(
      temp_dir.GetPath(), nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  ASSERT_TRUE(original_profile);
  auto profile_owner1 = RegistrationOwner(manager_, original_profile.get());
  auto* otr_profile1 = profile_owner1.profile();

  auto profile_owner2 = RegistrationOwner(manager_, original_profile.get());
  auto* otr_profile2 = profile_owner2.profile();

  watcher1.Watch(otr_profile1);
  watcher2.Watch(otr_profile2);
  // Run tasks to ensure that Mojo connections are created before the profile is
  // destroyed.
  base::RunLoop().RunUntilIdle();
  original_profile.reset();
  // |original_profile| being destroyed should trigger the dependent OTR
  // profiles to be destroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher1.destroyed());
  EXPECT_TRUE(watcher2.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       BrowserClosingDoesntRemoveProfileObserver) {
  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
#if defined(OS_CHROMEOS)
  EnableProfileHelperTestSettings();
#endif
  auto original_profile = Profile::CreateProfile(
      temp_dir.GetPath(), nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  ASSERT_TRUE(original_profile);
  auto profile_owner1 = RegistrationOwner(manager_, original_profile.get());
  auto* otr_profile1 = profile_owner1.profile();
  Browser* otr_browser = nullptr;
  {
    auto profile_owner2 = RegistrationOwner(manager_, original_profile.get());
    auto* otr_profile2 = profile_owner2.profile();

    otr_browser = CreateBrowser(otr_profile2);
    watcher2.Watch(otr_profile2);
  }
  base::RunLoop run_loop;
  BrowserRemovedWaiter removed_waiter(otr_browser,
                                      run_loop.QuitWhenIdleClosure());
  otr_browser->window()->Close();
  run_loop.Run();
  ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser));
  EXPECT_TRUE(watcher2.destroyed());

  watcher1.Watch(otr_profile1);
  original_profile.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher1.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       CallbackNotCalledAfterUnregister) {
  ProfileDestructionWatcher watcher;
  Browser* otr_browser = nullptr;
  Profile* otr_profile = nullptr;
  {
    auto profile_registration = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    otr_profile = profile_registration->profile();

    otr_browser = CreateBrowser(otr_profile);
  }
  watcher.Watch(otr_profile);
  base::RunLoop run_loop;
  BrowserRemovedWaiter removed_waiter(otr_browser,
                                      run_loop.QuitWhenIdleClosure());
  otr_browser->window()->Close();
  run_loop.Run();
  ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), otr_browser));
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest, Notifications) {
  // Create the OTR profile.
  content::WindowedNotificationObserver profile_created_observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  auto* profile = browser()->profile();
  auto profile_registration = manager_->CreateFromOriginalProfile(
      profile, base::BindOnce(&OriginalProfileNeverDestroyed));
  profile_created_observer.Wait();

  // Verify the received notification.
  auto* otr_profile = profile_registration->profile();
  EXPECT_EQ(profile_created_observer.source(),
            content::Source<Profile>(otr_profile));
  EXPECT_FALSE(profile->HasOffTheRecordProfile());
  EXPECT_TRUE(otr_profile->IsOffTheRecord());
  EXPECT_TRUE(otr_profile->IsIndependentOffTheRecordProfile());
}
