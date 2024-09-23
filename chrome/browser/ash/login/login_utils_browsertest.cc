// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#include "content/public/browser/browser_thread.h"
#endif

namespace ash {
namespace {

constexpr char kTestBrand[] = "TEST";

#if BUILDFLAG(ENABLE_RLZ)
void GetAccessPointRlzInBackgroundThread(rlz_lib::AccessPoint point,
                                         std::u16string* rlz) {
  ASSERT_FALSE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ASSERT_TRUE(rlz::RLZTracker::GetAccessPointRlz(point, rlz));
}
#endif

}  // namespace

class LoginUtilsTest : public LoginManagerTest {
 public:
  LoginUtilsTest() {
    scoped_fake_statistics_provider_.SetMachineStatistic(
        system::kRlzBrandCodeKey, kTestBrand);
  }

  LoginUtilsTest(const LoginUtilsTest&) = delete;
  LoginUtilsTest& operator=(const LoginUtilsTest&) = delete;

  ~LoginUtilsTest() override = default;

  PrefService* local_state() { return g_browser_process->local_state(); }

  LoginManagerMixin login_manager_{&mixin_host_};

 private:
  system::ScopedFakeStatisticsProvider scoped_fake_statistics_provider_;
};

#if BUILDFLAG(ENABLE_RLZ)
IN_PROC_BROWSER_TEST_F(LoginUtilsTest, RlzInitialized) {
  // No RLZ brand code set initially.
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kRLZBrand));

  // Wait for blocking RLZ tasks to complete.
  {
    base::RunLoop loop;
    login_manager_.SkipPostLoginScreens();
    EXPECT_FALSE(UserSessionInitializer::Get()->get_inited_for_testing());
    UserSessionInitializer::Get()->set_init_rlz_impl_closure_for_testing(
        loop.QuitClosure());

    login_manager_.LoginAsNewRegularUser();
    login_manager_.WaitForActiveSession();

    loop.Run();
  }

  // RLZ brand code has been set.
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kRLZBrand));
  EXPECT_EQ(local_state()->GetString(prefs::kRLZBrand), kTestBrand);

  // RLZ value for homepage access point should have been initialized.
  // This value must be obtained in a background thread.
  {
    base::RunLoop loop;
    std::u16string rlz_string;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&GetAccessPointRlzInBackgroundThread,
                       rlz::RLZTracker::ChromeHomePage(), &rlz_string),
        loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(std::u16string(), rlz_string);
  }
}
#endif

}  // namespace ash
