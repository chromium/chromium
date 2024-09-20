// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/toast/system_nudge_view.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/growth/show_notification_action_performer.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/env.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
using NudgeTestVariantsParam = std::tuple</*tablet_mode=*/bool,
                                          /*anchor_type_window_bounds=*/bool>;

constexpr char kCampaignsFileName[] = "campaigns.json";

constexpr char kEmptyCampaigns[] = R"(
{
}
)";

// Targeting Personalization App.
constexpr char kCampaignsNudgeTemplate[] = R"(
{
  "2": [
    {
      "id": 100,
      "targetings": [
        {
          "runtime": {
            "appsOpened": [
              {"appId": "glenkcidjgckcomnliblmkokolehpckn"}
            ]
          }
        }
      ],
      "payload": {
        "nudge": {
          "title": "Title",
          "body": "Body",
          "duration": 2,
          "image": {
            "builtInIcon": 0
          },
          "arrow": %s,
          "anchor": {
            "activeAppWindowAnchorType": %s
          },
          "primaryButton": {
            "label": "Yes",
            "action": {
              "type": 3,
              "params": {
                "url": "https://www.google.com",
                "disposition": 0
              }
            },
            "shouldMarkDismissed": true
          },
          "secondaryButton": {
            "label": "No",
            "action": {},
            "shouldMarkDismissed": true
          }
        }
      }
    }
  ]
}
)";

constexpr char kCampaignsNotification[] = R"(
{
  "3": [
    {
      "id": 101,
      "targetings": [
        {
          "runtime": {
            "triggerList": [
              {
                "triggerType": 1
              }
            ]
          }
        }
      ],
      "payload": {
        "notification": {
          "title": "Rebuy title",
          "message": "Rebuy message",
          "sourceIcon": {
            "builtInVectorIcon": 0
          },
          "image": {
            "builtInImage": 2
          },
          "shouldMarkDismissOnClose": true,
          "buttons": [
            {
              "label": "Get Perk",
              "shouldMarkDismissed": true,
              "action": {
                "type": 3,
                "params": {
                  "url": "https://www.google.com",
                  "disposition": 0
                }
              }
            },
            {
              "label": "Dismiss",
              "shouldMarkDismissed": true,
              "action": {
                "type": 0
              }
            }
          ]
        }
      }
    }
  ]
}
)";

base::FilePath GetCampaignsFilePath(const base::ScopedTempDir& dir) {
  return dir.GetPath().Append(kCampaignsFileName);
}

class TestCampaignsManagerObserver : public growth::CampaignsManager::Observer {
 public:
  // Spins a RunLoop until campaigns are loaded.
  void wait() {
    if (loaded_) {
      return;
    }
    run_loop_.Run();
  }

  void OnCampaignsLoadCompleted() override {
    loaded_ = true;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  bool loaded_ = false;
};

}  // namespace

// CampaignsManagerInteractiveUiTest -------------------------------------------

class CampaignsManagerInteractiveUiTest : public InteractiveAshTest {
 public:
  CampaignsManagerInteractiveUiTest()
      : animation_duration_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kGrowthCampaignsInConsumerSession);
    CHECK(temp_dir_.CreateUniqueTempDir());

    base::WriteFile(GetCampaignsFilePath(temp_dir_), kEmptyCampaigns);
  }

  // InteractiveBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchNative(ash::switches::kGrowthCampaignsPath,
                                     temp_dir_.GetPath().value());

    InteractiveAshTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();
    InteractiveAshTest::SetupContextWidget();

    WaitForCampaignLoaded();
  }

  void TearDownOnMainThread() override {
    if (InTabletMode()) {
      ash::TabletModeControllerTestApi().LeaveTabletMode();
    }
    InteractiveAshTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&CampaignsManagerInteractiveUiTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker = std::make_unique<
        testing::NiceMock<feature_engagement::test::MockTracker>>();

    ON_CALL(*mock_tracker, AddOnInitializedCallback)
        .WillByDefault(
            [](feature_engagement::Tracker::OnInitializedCallback callback) {
              std::move(callback).Run(true);
            });

    ON_CALL(*mock_tracker, IsInitialized).WillByDefault(testing::Return(true));

    return mock_tracker;
  }

 protected:
  void WaitForCampaignLoaded() {
    auto* campaigns_manager = growth::CampaignsManager::Get();
    ASSERT_TRUE(campaigns_manager);
    observer_ = std::make_unique<TestCampaignsManagerObserver>();
    campaigns_manager->AddObserver(observer_.get());
    observer_->wait();
  }

  auto CheckHistogramCounts(const std::string& name,
                            int sample,
                            int expected_count) {
    return Do([=, this]() {
      histogram_tester_.ExpectUniqueSample(name, sample, expected_count);
    });
  }

  auto SetTabletMode(const bool enable) {
    return Do([=, this]() {
      if (InTabletMode() == enable) {
        return;
      }
      enable ? ash::TabletModeControllerTestApi().EnterTabletMode()
             : ash::TabletModeControllerTestApi().LeaveTabletMode();
      CHECK_EQ(InTabletMode(), enable);
    });
  }

  auto ToggleTabletMode() { return SetTabletMode(!InTabletMode()); }

  feature_engagement::test::MockTracker* GetMockTracker() {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
            GetActiveUserProfile()));
  }

  base::ScopedTempDir temp_dir_;

 private:
  bool InTabletMode() { return display::Screen::GetScreen()->InTabletMode(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  base::HistogramTester histogram_tester_;
  ui::ScopedAnimationDurationScaleMode animation_duration_;
  std::unique_ptr<TestCampaignsManagerObserver> observer_;
  base::WeakPtrFactory<CampaignsManagerInteractiveUiTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventImpression) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_Campaign100_Impression";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->RecordEvent(
      GetEventName(growth::CampaignEvent::kImpression, "100"));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventDismissal) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_Campaign100_Dismissed";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->RecordEvent(
      GetEventName(growth::CampaignEvent::kDismissed, "100"));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventGroupImpression) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_Group10_Impression";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->RecordEvent(
      GetEventName(growth::CampaignEvent::kGroupImpression, "10"));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventGroupDismissal) {
  const std::string event_name = "ChromeOSAshGrowthCampaigns_Group10_Dismissed";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->RecordEvent(
      GetEventName(growth::CampaignEvent::kGroupDismissed, "10"));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventAppOpened) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_AppOpened_AppId_abcd";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->RecordEvent(
      GetEventName(growth::CampaignEvent::kAppOpened, "abcd"));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest, ClearConfig) {
  EXPECT_CALL(*GetMockTracker(), ClearEventData).Times(1);

  growth::CampaignsManager::Get()->ClearEvent(growth::CampaignEvent::kAppOpened,
                                              "abcd");
}

// CampaignsManagerInteractiveUiNudgeTest ----------------------------------

class CampaignsManagerInteractiveUiNudgeTest
    : public CampaignsManagerInteractiveUiTest,
      public testing::WithParamInterface<NudgeTestVariantsParam> {
 public:
  CampaignsManagerInteractiveUiNudgeTest() {
    std::string arrow = AnchorToWindowBounds() ? "2" : "1";
    std::string anchor_type = AnchorToWindowBounds() ? "1" : "0";
    base::WriteFile(GetCampaignsFilePath(temp_dir_),
                    base::StringPrintf(kCampaignsNudgeTemplate, arrow.c_str(),
                                       anchor_type.c_str()));
  }

  void SetUpOnMainThread() override {
    CampaignsManagerInteractiveUiTest::SetUpOnMainThread();

    // Use SWA as targets and anchors in the tests.
    InstallSystemApps();
  }

 protected:
  auto LaunchSystemWebApp(ash::SystemWebAppType type) {
    return Steps(
        Do([=, this]() {
          ash::LaunchSystemWebAppAsync(GetActiveUserProfile(), type);
        }),
        std::move(
            WaitForShow(kBrowserViewElementId).SetTransitionOnlyOnEvent(true)));
  }

  bool ShouldUseTabletMode() { return std::get<0>(GetParam()); }

  bool AnchorToWindowBounds() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CampaignsManagerInteractiveUiNudgeTest,
    testing::Combine(/*tablet_mode=*/testing::Bool(),
                     /*anchor_type_window_bounds=*/testing::Bool()),
    [](const testing::TestParamInfo<NudgeTestVariantsParam>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "TabletModeEnabled" : "TabletModeDisabled",
           "_",
           std::get<1>(info.param) ? "AnchorInsideWindowBounds"
                                   : "AnchorToCaptionButtonContainer"});
    });

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNudgeTest,
                       AnchorPersonalizationApp) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION),
      WaitForWindowWithTitle(env, u"Wallpaper & style"),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100, 0),
          ToggleTabletMode())));
}

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNudgeTest,
                       NotShowOnSettingsApp) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      LaunchSystemWebApp(ash::SystemWebAppType::SETTINGS),
      WaitForWindowWithTitle(env, u"Settings"),
      EnsureNotPresent(ash::SystemNudgeView::kBubbleIdForTesting),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100, 0),
          ToggleTabletMode())));
}

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNudgeTest,
                       ClickPrimaryButtonInAnchoredNudge) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting),
      PressButton(ash::SystemNudgeView::kPrimaryButtonIdForTesting),
      WaitForHide(ash::SystemNudgeView::kBubbleIdForTesting),
      WaitForWindowWithTitle(env, u"www.google.com"),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               1))));
}

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNudgeTest,
                       ClickSecondaryButtonInAnchoredNudge) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(Steps(
      SetTabletMode(ShouldUseTabletMode()),
      LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting),
      PressButton(ash::SystemNudgeView::kSecondaryButtonIdForTesting),
      WaitForHide(ash::SystemNudgeView::kBubbleIdForTesting),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 1),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               1)))));
}

// CampaignsManagerInteractiveUiNotificationTest
// ----------------------------------

class CampaignsManagerInteractiveUiNotificationTest
    : public CampaignsManagerInteractiveUiTest,
      public testing::WithParamInterface<bool> {
 public:
  CampaignsManagerInteractiveUiNotificationTest() {
    base::WriteFile(GetCampaignsFilePath(temp_dir_), kCampaignsNotification);
  }

 protected:
  message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        "growth_campaign_101");
  }

  // If a notification with `notification_id` is displayed, simulates clicking
  // on that notification with `button_index` button.
  auto Click(std::optional<int> button_index) {
    return Do([=, this]() {
      GetNotification()->delegate()->Click(button_index, std::nullopt);
    });
  }

  bool ShouldUseTabletMode() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CampaignsManagerInteractiveUiNotificationTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNotificationTest,
                       ShowNotification) {
  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      WaitForShow(ShowNotificationActionPerformer::kBubbleIdForTesting),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 101, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 101, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 101, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 101, 0),
          ToggleTabletMode())));
}

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNotificationTest,
                       ClickPrimaryButtonOnNotification) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      WaitForShow(ShowNotificationActionPerformer::kBubbleIdForTesting),
      Click(/*button_index=*/0),
      WaitForHide(ShowNotificationActionPerformer::kBubbleIdForTesting),
      WaitForWindowWithTitle(env, u"www.google.com"),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 101, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 101, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 101, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 101,
                               0))));
}

IN_PROC_BROWSER_TEST_P(CampaignsManagerInteractiveUiNotificationTest,
                       ClickSecondaryButtonOnNotification) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      SetTabletMode(ShouldUseTabletMode()),
      WaitForShow(ShowNotificationActionPerformer::kBubbleIdForTesting),
      Click(/*button_index=*/1),
      WaitForHide(ShowNotificationActionPerformer::kBubbleIdForTesting),
      WithoutDelay(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 101, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 101, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 101, 1),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 101,
                               0))));
}
