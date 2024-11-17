// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/current_tab_desktop_media_list.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;
using content::WebContentsTester;
using testing::_;
using testing::StrictMock;

namespace {

const base::TimeDelta kUpdatePeriod = base::Milliseconds(1000);

}  // namespace

ACTION_P(QuitMessageLoop, run_loop) {
  run_loop->QuitWhenIdle();
}

class CurrentTabDesktopMediaListTest : public testing::Test {
 protected:
  CurrentTabDesktopMediaListTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  CurrentTabDesktopMediaListTest(const CurrentTabDesktopMediaListTest&) =
      delete;
  CurrentTabDesktopMediaListTest& operator=(
      const CurrentTabDesktopMediaListTest&) = delete;

  void SetUp() override {
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));

    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kNoFirstRun);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    cl->AppendSwitch(switches::kTestType);
#endif

    profile_ = g_browser_process->profile_manager()
                   ->GetLastUsedProfileAllowedByPolicy();

    Browser::CreateParams profile_params(profile_, true);
    browser_ = CreateBrowserWithTestWindowForParams(profile_params);

    // Seed some tabs.
    for (int i = 0; i < 10; ++i) {  // Arbitrary value. kMainTab must be lower.
      CreateWebContents();
    }

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    list_.reset();

    // TODO(crbug.com/40571733): Tearing down the TabStripModel should just
    // delete all its owned WebContents. Then |manually_added_web_contents_|
    // won't be necessary.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    for (WebContents* contents : all_web_contents_) {
      tab_strip_model->DetachAndDeleteWebContentsAt(
          tab_strip_model->GetIndexOfWebContents(contents));
    }
    all_web_contents_.clear();

    browser_.reset();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    rvh_test_enabler_.reset();
  }

  std::unique_ptr<CurrentTabDesktopMediaList> CreateCurrentTabDesktopMediaList(
      WebContents* web_contents) {
    return base::WrapUnique(new CurrentTabDesktopMediaList(
        web_contents, kUpdatePeriod, &observer_));
  }

  void CreateWebContents() {
    std::unique_ptr<WebContents> web_contents(
        content::WebContentsTester::CreateTestWebContents(
            profile_, content::SiteInstance::Create(profile_)));

    // Get or create a NavigationEntry.
    content::NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    if (!entry) {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents.get(), GURL("chrome://blank"));
      entry = web_contents->GetController().GetLastCommittedEntry();
    }

    all_web_contents_.push_back(web_contents.get());
    browser_->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                   true);
  }

  void RemoveWebContents(WebContents* web_contents) {
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    tab_strip_model->DetachAndDeleteWebContentsAt(
        tab_strip_model->GetIndexOfWebContents(web_contents));
    std::erase(all_web_contents_, web_contents);
  }

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();  // Allow re-blocking.
  }

  void ResetLastHash() { list_->ResetLastHashForTesting(); }

  void RefreshList() { list_->Refresh(/*update_thumbnails=*/true); }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<Browser> browser_;

  StrictMock<DesktopMediaListMockObserver> observer_;
  std::unique_ptr<CurrentTabDesktopMediaList> list_;

  std::vector<raw_ptr<WebContents, VectorExperimental>> all_web_contents_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<base::RunLoop> run_loop_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          local_state_.Get(),
          ash::CrosSettings::Get())};
#endif
};

TEST_F(CurrentTabDesktopMediaListTest, UpdateSourcesListCalledWithCurrentTab) {
  constexpr size_t kMainTab = 3;
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(all_web_contents_[kMainTab]);
  run_loop_->Run();
}

TEST_F(CurrentTabDesktopMediaListTest,
       UpdateSourcesListNotCalledIfSourceAdded) {
  // Setup.
  constexpr size_t kMainTab = 3;
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(all_web_contents_[kMainTab]);
  run_loop_->Run();

  // Test focus.
  EXPECT_CALL(observer_, OnSourceAdded(_)).Times(0);  // Not called.
  CreateWebContents();
}

TEST_F(CurrentTabDesktopMediaListTest,
       UpdateSourcesListNotCalledIfSourceRemoved) {
  // Setup.
  constexpr size_t kMainTab = 3;
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(all_web_contents_[kMainTab]);
  run_loop_->Run();

  // Test focus.
  EXPECT_CALL(observer_, OnSourceRemoved(_)).Times(0);  // Not called.
  RemoveWebContents(all_web_contents_[kMainTab + 1]);
}

TEST_F(CurrentTabDesktopMediaListTest, OnSourceThumbnailCalledIfNewThumbnail) {
  // Setup.
  constexpr size_t kMainTab = 3;
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(all_web_contents_[kMainTab]);
  Wait();

  // Test focus.
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(_))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  ResetLastHash();  // Simulates the next frame being new.
  task_environment_.AdvanceClock(kUpdatePeriod);
  Wait();
}

TEST_F(CurrentTabDesktopMediaListTest,
       OnSourceThumbnailNotCalledIfIfOldThumbnail) {
  // Setup.
  constexpr size_t kMainTab = 3;
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(all_web_contents_[kMainTab]);
  Wait();

  // Test focus.
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(_)).Times(0);
  task_environment_.AdvanceClock(kUpdatePeriod);
}

TEST_F(CurrentTabDesktopMediaListTest, CallingRefreshAfterTabFreedIsSafe) {
  constexpr size_t kMainTab = 3;
  WebContents* const web_contents = all_web_contents_[kMainTab];

  // Setup.
  EXPECT_CALL(observer_, OnSourceAdded(0)).Times(1);
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .Times(1)
      .WillOnce(QuitMessageLoop(run_loop_.get()));
  list_ = CreateCurrentTabDesktopMediaList(web_contents);
  Wait();

  // Simulate tab closing.
  RemoveWebContents(web_contents);

  // Test focus - no crash.
  RefreshList();
}

// TODO(crbug.com/40724504): Test rescaling of the thumbnails.
