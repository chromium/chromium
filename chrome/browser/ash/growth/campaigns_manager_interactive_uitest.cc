// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kCampaignsFileName[] = "campaigns.json";

constexpr char kEmptyCampaigns[] = R"(
{
}
)";

base::FilePath GetCampaignsFilePath(const base::ScopedTempDir& dir) {
  return dir.GetPath().Append(kCampaignsFileName);
}

}  // namespace

class CampaignsManagerInteractiveUiTest : public InteractiveAshTest {
 public:
  CampaignsManagerInteractiveUiTest() {
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
  feature_engagement::test::MockTracker* GetMockTracker() {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
            GetActiveUserProfile()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::CallbackListSubscription create_services_subscription_;
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
