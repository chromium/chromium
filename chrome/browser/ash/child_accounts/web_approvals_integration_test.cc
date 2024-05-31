// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/supervised_user_integration_base_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/aura/env.h"

// Tests using production GAIA can only run on branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

class WebApprovalsIntegrationTest : public SupervisedUserIntegrationBaseTest {
 public:
  auto OpenBlockedSite(const std::string& url) {
    return Do([&]() { CreateBrowserWindow(GURL(url)); });
  }

  StateChange VerifyPageBlocked() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPageBlocked);
    StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kPageBlocked;
    state_change.test_function = "() => document.title === 'Site blocked'";
    return state_change;
  }

  std::string GetMatureSite() { return delegate_.test_data().mature_site; }
};

// Flaky: b/334993995
IN_PROC_BROWSER_TEST_F(WebApprovalsIntegrationTest,
                       DISABLED_TestMatureSiteBlocked) {
  SetupContextWidget();
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();
  ASSERT_TRUE(login_mixin().IsCryptohomeMounted());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBlockedTabId);

  const DeepQuery kLocalApprovalsButtonQuery{"#local-approvals-button"};

  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      Log("Navigate to mature site"),
      InstrumentNextTab(kBlockedTabId, AnyBrowser()),
      OpenBlockedSite(GetMatureSite()),

      Log("Check that local approvals button exists"),
      WaitForStateChange(kBlockedTabId, VerifyPageBlocked()),
      WaitForElementExists(kBlockedTabId, kLocalApprovalsButtonQuery));
}

}  // namespace ash

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
