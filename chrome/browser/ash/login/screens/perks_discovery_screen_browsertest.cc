// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service_factory.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kCampaignsFileName[] = "campaigns.json";

constexpr char kEmptyCampaigns[] = R"(
{
}
)";

constexpr char kCampaignsWithoutOOBE[] = R"(
{
  "0": [
        {
            "id": 1,
            "targetings": [],
            "payload": {}
        }
      ],
  "2": [
        {
          "id": 6,
          "studyId": 3,
          "groupId": 0,
          "targetings": [],
          "payload": {}
        }
      ],
}
)";

constexpr char kCampaignsOOBEPerksPayload[] = R"(
{
    "4": [
        {
            "id": 1,
            "targetings": [],
            "payload": {
                "oobePerkDiscovery": {
                    "perks": [
                        {
                            "id": "google_one",
                            "title": "Get 100G of cloud storage",
                            "text": "Your Chromebook comes with 100GB of cloud storage. Enjoy plenty of space for all your files and photos with 12 months of Google One at no cost. Term apply.",
                            "icon": "https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_icon.svg",
                            "content": {
                                "illustration": {
                                    "height": "342px",
                                    "width": "406px",
                                    "url": "https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_illustration.svg"
                                }
                            },
                            "primaryButton": {
                                "label": "Get perk after setup",
                                "action": {
                                    "type": 6,
                                    "params": {
                                        "userPref": "oobe.perk",
                                        "type": "Append",
                                        "value": "G1"
                                    }
                                }
                            },
                            "secondaryButton": {
                                "label": "Not interested"
                            }
                        }
                    ]
                }
            }
        }
    ]
}
)";

constexpr char kCampaignsOOBEPerksMalformedPayload[] = R"(
{
    "4": [
        {
            "id": 1,
            "targetings": [],
            "payload": {
                "oobePerkDiscovery": {
                    "perks": [
                        {
                            "id": "google_one",
                            "title": "Get 100G of cloud storage",
                            "text": "Your Chromebook comes with 100GB of cloud storage. Enjoy plenty of space for all your files and photos with 12 months of Google One at no cost. Term apply.",
                            "icon": "https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_icon.svg",
                            "content": {
                                "illustration": {
                                    "height": "342px",
                                    "url": "https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_illustration.svg"
                                }
                            },
                            "primaryButton": {
                                "label": "Get perk after setup",
                                "action": {
                                    "type": 6,
                                    "params": {
                                        "userPref": "oobe.perk",
                                        "type": "Append",
                                        "value": "G1"
                                    }
                                }
                            },
                            "secondaryButton": {
                                "label": "Not interested"
                            }
                        }
                    ]
                }
            }
        }
    ]
}
)";

constexpr char kPerksDiscoveryId[] = "perks-discovery";

const test::UIPath kDialogPath = {kPerksDiscoveryId, "perkDiscovery"};

const test::UIPath kSkipButton = {kPerksDiscoveryId, "perk-skip-button"};
const test::UIPath kNextButton = {kPerksDiscoveryId, "perk-next-button"};

base::FilePath GetCampaignsFilePath(const base::ScopedTempDir& dir) {
  return dir.GetPath().Append(kCampaignsFileName);
}

}  // namespace

class PerksDiscoveryScreenTest : public OobeBaseTest {
 public:
  PerksDiscoveryScreenTest() {
    feature_list_.InitWithFeatures({ash::features::kOobePerksDiscovery}, {});
    CHECK(temp_dir_.CreateUniqueTempDir());

    base::WriteFile(GetCampaignsFilePath(temp_dir_),
                    kCampaignsOOBEPerksPayload);
  }

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchNative(ash::switches::kGrowthCampaignsPath,
                                     temp_dir_.GetPath().value());
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    PerksDiscoveryScreen* perks_discovery_screen =
        WizardController::default_controller()
            ->GetScreen<PerksDiscoveryScreen>();

    original_callback_ =
        perks_discovery_screen->get_exit_callback_for_testing();
    perks_discovery_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    perks_discovery_screen->set_delay_for_overview_step_for_testing(
        base::Milliseconds(1));

    OobeBaseTest::SetUpOnMainThread();
  }

  void PerformLogin() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  }

  void ShowPerksDoscoveryScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        PerksDiscoveryScreenView::kScreenId);
  }

  PerksDiscoveryScreen::Result WaitForScreenExitResult() {
    PerksDiscoveryScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<PerksDiscoveryScreen::Result> screen_result_waiter_;
  PerksDiscoveryScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenTest, Next) {
  PerformLogin();
  ShowPerksDoscoveryScreen();

  OobeScreenWaiter(PerksDiscoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();
  test::OobeJS().TapOnPath(kNextButton);

  // TODO(b/356349576) Check that the preference is set correctly.
  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kNext);
}

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenTest, Skip) {
  PerformLogin();
  ShowPerksDoscoveryScreen();

  OobeScreenWaiter(PerksDiscoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();
  test::OobeJS().TapOnPath(kSkipButton);

  // TODO(b/356349576) Check that the preference is set correctly.
  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kNext);
}

class PerksDiscoveryScreenEmptyTest : public PerksDiscoveryScreenTest {
 public:
  PerksDiscoveryScreenEmptyTest() {
    base::WriteFile(GetCampaignsFilePath(temp_dir_), kEmptyCampaigns);
  }
};

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenEmptyTest, EmptyPayload) {
  PerformLogin();
  ShowPerksDoscoveryScreen();
  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kError);
}

class PerksDiscoveryScreenMalformedTest : public PerksDiscoveryScreenTest {
 public:
  PerksDiscoveryScreenMalformedTest() {
    base::WriteFile(GetCampaignsFilePath(temp_dir_),
                    kCampaignsOOBEPerksMalformedPayload);
  }
};

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenMalformedTest, MalformedPayload) {
  PerformLogin();
  ShowPerksDoscoveryScreen();
  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kError);
}

class PerksDiscoveryScreenNoOobeSlotTest : public PerksDiscoveryScreenTest {
 public:
  PerksDiscoveryScreenNoOobeSlotTest() {
    base::WriteFile(GetCampaignsFilePath(temp_dir_), kCampaignsWithoutOOBE);
  }
};

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenNoOobeSlotTest,
                       CampaignsWithoutOOBESlot) {
  PerformLogin();
  ShowPerksDoscoveryScreen();
  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kError);
}

class PerksDiscoveryScreenManagedTest : public PerksDiscoveryScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(PerksDiscoveryScreenManagedTest, SkipDueToManagedUser) {
  // Force the sync screen to be shown so that OOBE isn't destroyed
  // right after login due to all screens being skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();

  login_manager_mixin_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      PerksDiscoveryScreenView::kScreenId);

  PerksDiscoveryScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, PerksDiscoveryScreen::Result::kNotApplicable);
}

}  // namespace ash
