// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/toast/system_nudge_view.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/env.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace {

constexpr char kCampaignsFileName[] = "campaigns.json";

constexpr char kEmptyCampaigns[] = R"(
{
}
)";

// Targeting Personalization App.
constexpr char kCampaignsNudge[] = R"(
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
          "arrow": 1,
          "anchor": {
            "activeAppWindowAnchorType": 0
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

base::FilePath GetCampaignsFilePath(const base::ScopedTempDir& dir) {
  return dir.GetPath().Append(kCampaignsFileName);
}

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
  auto CheckHistogramCounts(const std::string& name,
                            int sample,
                            int expected_count) {
    return Do([=]() {
      histogram_tester_.ExpectUniqueSample(name, sample, expected_count);
    });
  }

  feature_engagement::test::MockTracker* GetMockTracker() {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
            GetActiveUserProfile()));
  }

  base::ScopedTempDir temp_dir_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  base::HistogramTester histogram_tester_;
  ui::ScopedAnimationDurationScaleMode animation_duration_;
  base::WeakPtrFactory<CampaignsManagerInteractiveUiTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventImpression) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_Campaign100_Impression";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->NotifyEventForTargeting(
      growth::CampaignEvent::kImpression, "100");
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventDismissal) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_Campaign100_Dismissed";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->NotifyEventForTargeting(
      growth::CampaignEvent::kDismissed, "100");
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest,
                       NotifyEventAppOpened) {
  const std::string event_name =
      "ChromeOSAshGrowthCampaigns_AppOpened_AppId_abcd";
  EXPECT_CALL(*GetMockTracker(), NotifyEvent(event_name)).Times(1);

  growth::CampaignsManager::Get()->NotifyEventForTargeting(
      growth::CampaignEvent::kAppOpened, "abcd");
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiTest, ClearConfig) {
  EXPECT_CALL(*GetMockTracker(), ClearEventData).Times(1);

  growth::CampaignsManager::Get()->ClearEvent(growth::CampaignEvent::kAppOpened,
                                              "abcd");
}

// CampaignsManagerInteractiveUiNudgeTest ----------------------------------

class CampaignsManagerInteractiveUiNudgeTest
    : public CampaignsManagerInteractiveUiTest {
 public:
  CampaignsManagerInteractiveUiNudgeTest() {
    base::WriteFile(GetCampaignsFilePath(temp_dir_), kCampaignsNudge);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    InteractiveAshTest::SetupContextWidget();

    // Use SWA as targets and anchors in the tests.
    InstallSystemApps();
  }

 protected:
  auto LaunchSystemWebApp(ash::SystemWebAppType type) {
    return Do(
        [=]() { ash::LaunchSystemWebAppAsync(GetActiveUserProfile(), type); });
  }
};

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiNudgeTest,
                       AnchorPersonalizationApp) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      InAnyContext(
          Steps(LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION))),
      WaitForWindowWithTitle(env, u"Wallpaper & style"),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting), FlushEvents(),
      InAnyContext(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               0))));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiNudgeTest,
                       NotShowOnSettingsApp) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      InAnyContext(Steps(LaunchSystemWebApp(ash::SystemWebAppType::SETTINGS))),
      WaitForWindowWithTitle(env, u"Settings"),
      EnsureNotPresent(ash::SystemNudgeView::kBubbleIdForTesting),
      FlushEvents(),
      InAnyContext(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               0))));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiNudgeTest,
                       ClickPrimaryButtonInAnchoredNudge) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      InAnyContext(
          Steps(LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION))),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting), FlushEvents(),
      PressButton(ash::SystemNudgeView::kPrimaryButtonIdForTesting),
      WaitForHide(ash::SystemNudgeView::kBubbleIdForTesting),
      WaitForWindowWithTitle(env, u"www.google.com"), FlushEvents(),
      InAnyContext(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 0),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               1))));
}

IN_PROC_BROWSER_TEST_F(CampaignsManagerInteractiveUiNudgeTest,
                       ClickSecondaryButtonInAnchoredNudge) {
  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(
      InAnyContext(
          Steps(LaunchSystemWebApp(ash::SystemWebAppType::PERSONALIZATION))),
      WaitForShow(ash::SystemNudgeView::kBubbleIdForTesting), FlushEvents(),
      PressButton(ash::SystemNudgeView::kSecondaryButtonIdForTesting),
      WaitForHide(ash::SystemNudgeView::kBubbleIdForTesting), FlushEvents(),
      InAnyContext(Steps(
          CheckHistogramCounts("Ash.Growth.Ui.Impression.Campaigns500", 100, 1),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500", 100, 0),
          CheckHistogramCounts(
              "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500", 100, 1),
          CheckHistogramCounts("Ash.Growth.Ui.Dismissed.Campaigns500", 100,
                               1))));
}
