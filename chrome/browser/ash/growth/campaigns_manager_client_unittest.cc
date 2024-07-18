// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

using ::component_updater::FakeComponentManagerAsh;

inline constexpr char kCampaignsComponent[] = "growth-campaigns";

inline constexpr char kTestCampaignsComponentMountedPath[] =
    "/run/imageloader/growth_campaigns";

constexpr char kButtonPressedButton0HistogramName500[] =
    "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns500";

constexpr char kButtonPressedButton0HistogramName1000[] =
    "Ash.Growth.Ui.ButtonPressed.Button0.Campaigns1000";

constexpr char kButtonPressedButton1HistogramName500[] =
    "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns500";

constexpr char kButtonPressedButton1HistogramName1000[] =
    "Ash.Growth.Ui.ButtonPressed.Button1.Campaigns1000";

constexpr char kDismissedHistogramName500[] =
    "Ash.Growth.Ui.Dismissed.Campaigns500";

constexpr char kDismissedHistogramName1000[] =
    "Ash.Growth.Ui.Dismissed.Campaigns1000";

constexpr char kImpressionHistogramName500[] =
    "Ash.Growth.Ui.Impression.Campaigns500";

constexpr char kImpressionHistogramName1000[] =
    "Ash.Growth.Ui.Impression.Campaigns1000";

}  // namespace

class CampaignsManagerClientTest : public testing::Test {
 public:
  CampaignsManagerClientTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}

  CampaignsManagerClientTest(const CampaignsManagerClientTest&) = delete;
  CampaignsManagerClientTest& operator=(const CampaignsManagerClientTest&) =
      delete;

  ~CampaignsManagerClientTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kGrowthCampaignsCrOSEvents);
    SetupProfileManager();
    InitializeComponentManager();
    campaigns_manager_client_ = std::make_unique<CampaignsManagerClientImpl>();
    metrics_recorder_.Initialize();
  }

  void TearDown() override {
    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
    campaigns_manager_client_.reset();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

 protected:
  void InitializeComponentManager() {
    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeComponentManagerAsh>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kCampaignsComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeComponentManager(
        std::move(fake_cros_component_manager));
  }

  bool FinishComponentLoad(
      const base::FilePath& mount_path,
      component_updater::FakeComponentManagerAsh::Error error) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kCampaignsComponent));
    EXPECT_TRUE(cros_component_manager_->UpdateRequested(kCampaignsComponent));

    auto install_path = base::FilePath();
    if (error == component_updater::FakeComponentManagerAsh::Error::NONE) {
      install_path = base::FilePath("/dev/null");
    }

    return cros_component_manager_->FinishLoadRequest(
        kCampaignsComponent,
        FakeComponentManagerAsh::ComponentInfo(
            /*load_response=*/error, install_path, mount_path));
  }

  void SetupProfileManager() {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void ValidateButtonPressedEvent(const metrics::structured::Event& event,
                                  int campaign_id,
                                  std::optional<int> group_id,
                                  CampaignButtonId button_id) {
    cros_events::Growth_Ui_ButtonPressed expected_event;
    expected_event.SetCampaignId(campaign_id)
        .SetButtonId(static_cast<cros_events::CampaignButtonId>(button_id));

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());
    EXPECT_EQ(expected_event.metric_values(), event.metric_values());

    // TODO: b/348495965 - Verify group metrics when ready.
  }

  void ValidateDismissedEvent(const metrics::structured::Event& event,
                              int campaign_id,
                              std::optional<int> group_id) {
    cros_events::Growth_Ui_Dismissed expected_event;
    expected_event.SetCampaignId(campaign_id);

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());
    EXPECT_EQ(expected_event.metric_values(), event.metric_values());

    // TODO: b/348495965 - Verify group metrics when ready.
  }

  void ValidateImpresionEvent(const metrics::structured::Event& event,
                              int campaign_id,
                              std::optional<int> group_id) {
    cros_events::Growth_Ui_Impression expected_event;
    expected_event.SetCampaignId(campaign_id);

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());
    EXPECT_EQ(expected_event.metric_values(), event.metric_values());

    // TODO: b/348495965 - Verify group metrics when ready.
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CampaignsManagerClientImpl> campaigns_manager_client_;
  raw_ptr<FakeComponentManagerAsh> cros_component_manager_ = nullptr;
  base::HistogramTester histogram_tester_;
  metrics::structured::TestStructuredMetricsRecorder metrics_recorder_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
};

TEST_F(CampaignsManagerClientTest, LoadCampaignsComponent) {
  campaigns_manager_client_->LoadCampaignsComponent(base::BindLambdaForTesting(
      [](const std::optional<const base::FilePath>& file_path) {
        ASSERT_TRUE(file_path.has_value());
        ASSERT_EQ(file_path.value().value(),
                  kTestCampaignsComponentMountedPath);
      }));

  ASSERT_TRUE(
      FinishComponentLoad(base::FilePath(kTestCampaignsComponentMountedPath),
                          component_updater::ComponentManagerAsh::Error::NONE));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerClientTest, LoadCampaignsComponentFailed) {
  campaigns_manager_client_->LoadCampaignsComponent(base::BindLambdaForTesting(
      [](const std::optional<const base::FilePath>& file_path) {
        ASSERT_FALSE(file_path.has_value());
      }));

  ASSERT_TRUE(FinishComponentLoad(
      base::FilePath(),
      component_updater::ComponentManagerAsh::Error::NOT_FOUND));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton0Id0) {
  int campaign_id = 0;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, NotRecordButtonPressedCrosEvents) {
  int campaign_id = 0;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/false);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 0U);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton0Id0NoDismissal) {
  int campaign_id = 0;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/false, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton0Id499) {
  int campaign_id = 499;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton0Id500) {
  int campaign_id = 500;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName1000,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton0Id0And500) {
  int campaign_id_0 = 0;
  int campaign_id_500 = 500;
  CampaignButtonId button_id = CampaignButtonId::kPrimary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id_0, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);
  campaigns_manager_client_->OnButtonPressed(
      campaign_id_500, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName500,
                                       campaign_id_0,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kButtonPressedButton0HistogramName1000,
                                       campaign_id_500,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 2U);
  ValidateButtonPressedEvent(events[0], campaign_id_0,
                             /*group_id=*/std::nullopt, button_id);
  ValidateButtonPressedEvent(events[1], campaign_id_500,
                             /*group_id=*/std::nullopt, button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton1Id0) {
  int campaign_id = 0;
  CampaignButtonId button_id = CampaignButtonId::kSecondary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton1Id0NoDismissal) {
  int campaign_id = 0;
  CampaignButtonId button_id = CampaignButtonId::kSecondary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/false, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton1Id499) {
  int campaign_id = 499;
  CampaignButtonId button_id = CampaignButtonId::kSecondary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName500,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton1Id500) {
  int campaign_id = 500;
  CampaignButtonId button_id = CampaignButtonId::kSecondary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName1000,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateButtonPressedEvent(events[0], campaign_id, /*group_id=*/std::nullopt,
                             button_id);
}

TEST_F(CampaignsManagerClientTest, RecordButtonPressedButton1Id0And500) {
  int campaign_id_0 = 0;
  int campaign_id_500 = 500;
  CampaignButtonId button_id = CampaignButtonId::kSecondary;
  campaigns_manager_client_->OnButtonPressed(
      campaign_id_0, /*group_id=*/std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);
  campaigns_manager_client_->OnButtonPressed(
      campaign_id_500, std::nullopt, button_id,
      /*should_mark_dismissed=*/true, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName500,
                                       campaign_id_0,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kButtonPressedButton1HistogramName1000,
                                       campaign_id_500,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 2U);
  ValidateButtonPressedEvent(events[0], campaign_id_0,
                             /*group_id=*/std::nullopt, button_id);
  ValidateButtonPressedEvent(events[1], campaign_id_500,
                             /*group_id=*/std::nullopt, button_id);
}

TEST_F(CampaignsManagerClientTest, RecordDismissedId0) {
  int campaign_id = 0;
  campaigns_manager_client_->OnDismissed(campaign_id, /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateDismissedEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, NotRecordDismissedCrosEvents) {
  int campaign_id = 0;
  campaigns_manager_client_->OnDismissed(campaign_id, /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/false);

  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 0U);
}

TEST_F(CampaignsManagerClientTest, RecordDismissedId499) {
  int campaign_id = 499;
  campaigns_manager_client_->OnDismissed(campaign_id, /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateDismissedEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, RecordDismissedId500) {
  int campaign_id = 500;
  campaigns_manager_client_->OnDismissed(campaign_id, /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName1000, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateDismissedEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, RecordDismissedId0And500) {
  int campaign_id_0 = 0;
  int campaign_id_500 = 500;
  campaigns_manager_client_->OnDismissed(campaign_id_0,
                                         /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/true);
  campaigns_manager_client_->OnDismissed(campaign_id_500,
                                         /*group_id=*/std::nullopt,
                                         /*should_mark_dismissed=*/false,
                                         /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName500,
                                       campaign_id_0,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kDismissedHistogramName1000,
                                       campaign_id_500,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 2U);
  ValidateDismissedEvent(events[0], campaign_id_0, /*group_id=*/std::nullopt);
  ValidateDismissedEvent(events[1], campaign_id_500, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, RecordImpressionId0) {
  int campaign_id = 0;
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateImpresionEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, NotRecordImpressionCrosEvents) {
  int campaign_id = 0;
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/false);

  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 0U);
}

TEST_F(CampaignsManagerClientTest, RecordImpressionId499) {
  int campaign_id = 499;
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName500, campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateImpresionEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, RecordImpressionId500) {
  int campaign_id = 500;
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName1000,
                                       campaign_id,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 1U);
  ValidateImpresionEvent(events[0], campaign_id, /*group_id=*/std::nullopt);
}

TEST_F(CampaignsManagerClientTest, RecordImpressionId0And500) {
  int campaign_id_0 = 0;
  int campaign_id_500 = 500;
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id_0,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/true);
  campaigns_manager_client_->OnReadyToLogImpression(
      campaign_id_500,
      /*group_id=*/std::nullopt, /*should_log_cros_events=*/true);

  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName500,
                                       campaign_id_0,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kImpressionHistogramName1000,
                                       campaign_id_500,
                                       /*expected_bucket_count=*/1);

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_.GetEvents();
  ASSERT_EQ(events.size(), 2U);
  ValidateImpresionEvent(events[0], campaign_id_0, /*group_id=*/std::nullopt);
  ValidateImpresionEvent(events[1], campaign_id_500, /*group_id=*/std::nullopt);
}
