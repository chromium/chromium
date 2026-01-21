// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "glic_metrics.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {
using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class MockDelegate : public GlicMetrics::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  // GlicMetrics::Delegate implementation
  bool IsWindowShowing() const override { return showing; }
  bool IsWindowAttached() const override { return attached; }
  gfx::Size GetWindowSize() const override { return gfx::Size(); }
  content::WebContents* GetFocusedWebContents() override {
    return contents_.get();
  }
  ActiveTabSharingState GetActiveTabSharingState() override {
    return tab_sharing_state;
  }
  int32_t GetNumPinnedTabs() const override { return num_pinned_tabs; }
  std::vector<content::WebContents*> GetPinnedAndSharedWebContents() override {
    return pinned_shared_tabs;
  }

  void SetFocusedWebContents(content::WebContents* contents) {
    contents_ = contents;
  }
  void AddToPinnedSharedTabs(content::WebContents* contents) {
    pinned_shared_tabs.push_back(contents);
  }

  bool showing = false;
  bool attached = false;
  ActiveTabSharingState tab_sharing_state =
      ActiveTabSharingState::kActiveTabIsShared;
  int32_t num_pinned_tabs = 0;
  std::vector<content::WebContents*> pinned_shared_tabs;

 private:
  raw_ptr<content::WebContents> contents_;
};

class MockStatusIcon : public StatusIcon {
 public:
  explicit MockStatusIcon(const std::u16string& tool_tip)
      : tool_tip_(tool_tip) {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {
    tool_tip_ = tool_tip;
  }
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {
    menu_item_ = menu;
  }
  const std::u16string& tool_tip() const { return tool_tip_; }
  StatusIconMenuModel* menu_item() const { return menu_item_; }

 private:
  raw_ptr<StatusIconMenuModel> menu_item_ = nullptr;
  std::u16string tool_tip_;
};

class MockStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>(tool_tip);
  }

  const StatusIcons& GetStatusIconsForTest() const { return status_icons(); }
};

class GlicMetricsTestBase : public testing::Test {
 public:
  GlicMetricsTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager_->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager_->CreateTestingProfile("profile");
    ForceSigninAndModelExecutionCapability(profile_);
  }

  void TearDown() override {
    // The order of some of these operations is important to avoid
    // dangling pointer crashes.
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    profile_ = nullptr;
    testing_profile_manager_.reset();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void ExpectEntryPointImpressionLogged(
      EntryPointStatus entry_point_impression) {
    task_environment_.FastForwardBy(base::Minutes(16));
    histogram_tester_.ExpectUniqueSample("Glic.EntryPoint.Status",
                                         entry_point_impression,
                                         /*expected_count=*/1);
  }

 protected:
  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::UserActionTester& user_action_tester() { return user_action_tester_; }

  ukm::TestAutoSetUkmRecorder& ukm_tester() { return ukm_tester_; }

  ProfileManager* profile_manager() {
    return testing_profile_manager_->profile_manager();
  }

  Profile* profile() { return profile_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kGlicClosedCaptioning};

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  content::RenderViewHostTestEnabler enabler_;

  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  ukm::TestAutoSetUkmRecorder ukm_tester_;

#if BUILDFLAG(IS_CHROMEOS)
  // This test needs to run in user session, so set it up for ChromeOS cases.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  signin::IdentityTestEnvironment identity_env_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

class GlicMetricsTest : public GlicMetricsTestBase {
 public:
  void SetUp() override {
    GlicMetricsTestBase::SetUp();

    enabling_ = std::make_unique<GlicEnabling>(
        profile(), &profile_manager()->GetProfileAttributesStorage());
    metrics_ = std::make_unique<GlicMetrics>(profile(), enabling_.get());
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    metrics_->SetDelegateForTesting(std::move(delegate));
  }

  void TearDown() override {
    delegate_ = nullptr;
    metrics_.reset();
    enabling_.reset();
    test_web_contents_.reset();
    GlicMetricsTestBase::TearDown();
  }

  void InitializeTestWebContents() {
    // Create a SiteInstance, which is required to build a WebContents.
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(profile());

    // Use WebContentsTester::CreateTestWebContents(...) to create a real
    // WebContents suitable for unit testing.
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), site_instance.get());
    auto* tester = content::WebContentsTester::For(test_web_contents_.get());

    GURL url("https://www.google.com");
    tester->NavigateAndCommit(url);
  }

  content::WebContents* test_web_contents() { return test_web_contents_.get(); }
  GlicMetrics* metrics() { return metrics_.get(); }
  MockDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<content::WebContents> test_web_contents_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
  // Owned by `metrics_`.
  raw_ptr<MockDelegate> delegate_ = nullptr;
};

TEST_F(GlicMetricsTest, Basic) {
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics()->OnResponseRated(/*positive=*/true);
  metrics()->OnSessionTerminated();

  histogram_tester().ExpectTotalCount("Glic.Response.StopTime", 1);
  histogram_tester().ExpectTotalCount("Glic.Response.StopTime.UnknownCause", 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Session.InputSubmit.BrowserActiveState", 5 /*kBrowserHidden*/, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Session.ResponseStart.BrowserActiveState", 5 /*kBrowserHidden*/, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnUserInputSubmitted",
      ActiveTabSharingState::kActiveTabIsShared, 1);
  EXPECT_THAT(
      histogram_tester().GetAllSamplesForPrefix("Glic.Response.StartTime"),
      IsEmpty());

  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStopUnknownCause"),
            1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponse"), 0);
}

TEST_F(GlicMetricsTest, BasicVisible) {
  delegate()->showing = true;
  delegate()->attached = true;

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics()->OnResponseRated(/*positive=*/true);
  metrics()->OnSessionTerminated();
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Response.StopTime", 1);
  histogram_tester().ExpectUniqueSample("Glic.Session.Open.BrowserActiveState",
                                        5 /*kBrowserHidden*/, 1);
  EXPECT_THAT(
      histogram_tester().GetAllSamplesForPrefix("Glic.Response.StartTime"),
      UnorderedElementsAre(
          Pair("Glic.Response.StartTime",
               BucketsAre(Bucket(/*time bucket*/ 0, 1))),
          Pair("Glic.Response.StartTime.InputMode.Text",
               BucketsAre(Bucket(/*time bucket*/ 0, 1))),
          Pair("Glic.Response.StartTime.TabContext.LikelyWithout",
               BucketsAre(Bucket(/*time bucket*/ 0, 1)))));
  EXPECT_EQ(user_action_tester().GetActionCount("GlicSessionBegin"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponse"), 1);
}

TEST_F(GlicMetricsTest, ResponseStartTime_WithFocusedTab) {
  delegate()->showing = true;
  delegate()->attached = true;
  delegate()->tab_sharing_state = ActiveTabSharingState::kActiveTabIsShared;
  InitializeTestWebContents();
  delegate()->SetFocusedWebContents(test_web_contents());

  metrics()->DidRequestContextFromTab(*test_web_contents());
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();

  EXPECT_THAT(
      histogram_tester().GetAllSamplesForPrefix("Glic.Response.StartTime"),
      UnorderedElementsAre(Pair("Glic.Response.StartTime",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1))),
                           Pair("Glic.Response.StartTime.InputMode.Text",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1))),
                           Pair("Glic.Response.StartTime.TabContext.LikelyWith",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1)))));
}

TEST_F(GlicMetricsTest, ResponseStartTime_WithPinnedAndSharedTab) {
  delegate()->showing = true;
  delegate()->attached = true;
  delegate()->tab_sharing_state =
      ActiveTabSharingState::kTabContextPermissionNotGranted;
  InitializeTestWebContents();
  delegate()->AddToPinnedSharedTabs(test_web_contents());

  metrics()->DidRequestContextFromTab(*test_web_contents());
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics()->OnResponseStarted();

  EXPECT_THAT(
      histogram_tester().GetAllSamplesForPrefix("Glic.Response.StartTime"),
      UnorderedElementsAre(Pair("Glic.Response.StartTime",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1))),
                           Pair("Glic.Response.StartTime.InputMode.Audio",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1))),
                           Pair("Glic.Response.StartTime.TabContext.LikelyWith",
                                BucketsAre(Bucket(/*time bucket*/ 0, 1)))));
}

TEST_F(GlicMetricsTest, BasicUkm) {
  delegate()->showing = true;
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kFre);
  for (int i = 0; i < 2; ++i) {
    metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
    metrics()->OnResponseStarted();
    metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  }

  {
    auto entries = ukm_tester().GetEntriesByName("Glic.WindowOpen");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    ukm_tester().ExpectEntryMetric(entry, "Attached", false);
    ukm_tester().ExpectEntryMetric(
        entry, "InvocationSource",
        static_cast<int64_t>(mojom::InvocationSource::kFre));
    auto* source = ukm_tester().GetSourceForSourceId(entry->source_id);
    EXPECT_FALSE(source);
  }

  {
    auto entries = ukm_tester().GetEntriesByName("Glic.Response");
    ASSERT_EQ(entries.size(), 2u);
    for (int i = 0; i < 2; ++i) {
      auto entry = entries[i];
      ukm_tester().ExpectEntryMetric(entry, "Attached", false);
      ukm_tester().ExpectEntryMetric(
          entry, "WebClientMode",
          static_cast<int64_t>(mojom::WebClientMode::kText));
      ukm_tester().ExpectEntryMetric(
          entry, "InvocationSource",
          static_cast<int64_t>(mojom::InvocationSource::kFre));
      auto* source = ukm_tester().GetSourceForSourceId(entry->source_id);
      EXPECT_FALSE(source);
    }
  }
}

TEST_F(GlicMetricsTest, BasicUkmWithTarget) {
  InitializeTestWebContents();
  delegate()->SetFocusedWebContents(test_web_contents());
  delegate()->showing = true;

  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kFre);
  metrics()->DidRequestContextFromTab(*test_web_contents());
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  ukm::SourceId ukm_id =
      test_web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  {
    auto entries = ukm_tester().GetEntriesByName("Glic.WindowOpen");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    // TODO(b/452120577): Source ID should match `ukm_id`.
    EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
  }

  {
    auto entries = ukm_tester().GetEntriesByName("Glic.Response");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    EXPECT_EQ(entry->source_id, ukm_id);
  }
}

TEST_F(GlicMetricsTest, BasicStopReasonOther) {
  delegate()->showing = true;
  delegate()->attached = true;

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kOther);
  metrics()->OnSessionTerminated();
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Response.StopTime.Other", 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStopOther"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStop"), 1);
}
TEST_F(GlicMetricsTest, BasicStopReasonByUser) {
  delegate()->showing = true;
  delegate()->attached = true;

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUser);
  metrics()->OnSessionTerminated();
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Response.StopTime.ByUser", 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponseStopByUser"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("GlicResponse"), 1);
}
TEST_F(GlicMetricsTest, SegmentationOsButtonAttachedText) {
  delegate()->showing = true;
  delegate()->attached = true;

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Response.Segmentation", ResponseSegmentation::kOsButtonAttachedText,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, Segmentation3DotsMenuDetachedAudio) {
  delegate()->showing = true;
  delegate()->attached = false;

  metrics()->OnGlicWindowStartedOpening(
      /*attached=*/false, mojom::InvocationSource::kThreeDotsMenu);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics()->OnResponseStarted();
  metrics()->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Response.Segmentation",
      ResponseSegmentation::kThreeDotsMenuDetachedAudio,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SessionDuration_LogsDuration) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  int minutes = 10;
  task_environment().FastForwardBy(base::Minutes(minutes));
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Session.Duration", 1);
  histogram_tester().ExpectTimeBucketCount(
      "Glic.Session.Duration", base::Minutes(minutes), /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SessionDuration_LogsError) {
  // Trigger a call to |OnGlicWindowClose()| without opening the window first.
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());

  histogram_tester().ExpectTotalCount("Glic.Session.Duration", 0);
  histogram_tester().ExpectTotalCount("Glic.Metrics.Error", 1);
  histogram_tester().ExpectBucketCount("Glic.Metrics.Error",
                                       Error::kWindowCloseWithoutWindowOpen,
                                       /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, ClosedCaptionsResponse_PrefLogsFalse) {
  metrics()->LogClosedCaptionsShown();

  histogram_tester().ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                        false, 1);
}

TEST_F(GlicMetricsTest, ClosedCaptionsResponse_PrefLogsTrue) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicClosedCaptioningEnabled, true);
  metrics()->LogClosedCaptionsShown();

  histogram_tester().ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                        true, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedSuccessful) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingSucceeded);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingSucceeded, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedUnsuccessfulTooMany) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedTooManyTabs);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedTooManyTabs, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedUnsuccessfulNotValid) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedNotValidForSharing);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedNotValidForSharing,
      1);
}

TEST_F(GlicMetricsTest, LogGetContextFromFocusedTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text", 0);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio", 0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextFromTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextFromTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text",
                                      0);
  histogram_tester().ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Audio",
                                      0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextForActorFromTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextForActorFromTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextForActorFromTab.Error.Text", 0);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextForActorFromTab.Error.Audio", 0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextForActorFromTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextFromFocusedTabError_ChangingModes) {
  // Simulates the client starting in text mode and later switching to audio.
  metrics()->SetWebClientMode(mojom::WebClientMode::kText);
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kWebContentsChanged);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kPermissionDenied);

  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Text",
      GlicGetContextFromTabError::kWebContentsChanged, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio",
      GlicGetContextFromTabError::kPermissionDenied, 1);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Unknown", 0);
}

TEST_F(GlicMetricsTest, ImpressionBeforeFreNotPermittedByPolicy) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreNotEligible);
}

TEST_F(GlicMetricsTest, ImpressionIncompleteFreNotPermittedByPolicy) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kIncompleteFreNotEligible);
}

// kGeminiSettings is by default enabled, however if we initialize a scoped
// feature list in a test, since the features were initially off during setup,
// glic is considered disabled until the kGeminiSettings pref changes and
// subscribers are notified. The following tests turn the feature flags on
// before setup happens, so that glic is enabled from the start.
class GlicMetricsFeaturesEnabledTest : public GlicMetricsTestBase {
 private:
  GlicUnitTestEnvironment glic_test_env_;
};

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionBeforeFre) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreAndEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionIncompleteFre) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kIncompleteFreAndEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreBrowserOnly) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreBrowserOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreOsOnly) {
  // kGeminiSettings is enabled
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreOsOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreEnabled) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreBrowserAndOs);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreDisabledEntrypoints) {
  // kGeminiSettings is enabled
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreThreeDotOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreNotPermittedByPolicy) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is true

  // Disable kGeminiSettings
  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreNotEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, EnablingChanged) {
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 0);
  // Glic starts enabled and during profile creation GlicMetrics records a user
  // action.
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 1);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 1);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 2);

  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 2);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 2);

  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 2);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 3);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 3);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Enabled"), 3);
}

TEST_F(GlicMetricsFeaturesEnabledTest, PinnedChanged) {
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 0);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 1);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ShortcutStatus) {
  task_environment().FastForwardBy(base::Minutes(16));
  histogram_tester().ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 1);
  histogram_tester().ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*true*/ 1,
      /*expected_count=*/1);

  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(ui::Accelerator()));

  task_environment().FastForwardBy(base::Minutes(16));
  histogram_tester().ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 2);
  histogram_tester().ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*false*/ 0,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, InputModesUsed) {
  // TODO(b/452378389): Unconventional order of metrics calls may be a problem.
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.InputModesUsed", 1);
  histogram_tester().ExpectBucketCount("Glic.Session.InputModesUsed",
                                       InputModesUsed::kOnlyText, 1);

  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.InputModesUsed", 2);
  histogram_tester().ExpectBucketCount("Glic.Session.InputModesUsed",
                                       InputModesUsed::kNone, 1);

  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.InputModesUsed", 3);
  histogram_tester().ExpectBucketCount("Glic.Session.InputModesUsed",
                                       InputModesUsed::kTextAndAudio, 1);

  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.InputModesUsed", 4);
  histogram_tester().ExpectBucketCount("Glic.Session.InputModesUsed",
                                       InputModesUsed::kOnlyAudio, 1);
}

TEST_F(GlicMetricsTest, AttachStateChanges) {
  // TODO(b/452378389): Unconventional order of metrics calls may be a problem.
  // Attach changes during initialization should not be counted.
  metrics()->OnAttachedToBrowser(AttachChangeReason::kInit);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.AttachStateChanges", 1);
  histogram_tester().ExpectBucketCount("Glic.Session.AttachStateChanges", 0, 1);

  metrics()->OnAttachedToBrowser(AttachChangeReason::kDrag);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.AttachStateChanges", 2);
  histogram_tester().ExpectBucketCount("Glic.Session.AttachStateChanges", 1, 1);

  metrics()->OnAttachedToBrowser(AttachChangeReason::kMenu);
  metrics()->OnDetachedFromBrowser(AttachChangeReason::kMenu);
  metrics()->OnAttachedToBrowser(AttachChangeReason::kMenu);
  metrics()->OnDetachedFromBrowser(AttachChangeReason::kMenu);
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  histogram_tester().ExpectTotalCount("Glic.Session.AttachStateChanges", 3);
  histogram_tester().ExpectBucketCount("Glic.Session.AttachStateChanges", 4, 1);
}

TEST_F(GlicMetricsTest, TimeElapsedBetweenSessions) {
  base::TimeDelta elapsed_time = base::Hours(2);

  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  task_environment().FastForwardBy(elapsed_time);

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  histogram_tester().ExpectTotalCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions", elapsed_time.InSeconds(),
      1);
}

TEST_F(GlicMetricsTest, PositionOnOpenAndClose) {
  // TODO(b/452378389): Unconventional order of metrics calls may be a problem.
  display::Display display;
  display.set_bounds(gfx::Rect(300, 350));
  display.set_work_area(gfx::Rect(0, 50, 300, 300));
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(50, 50, 0, 0));
  metrics()->OnGlicWindowClose(nullptr, display, gfx::Rect(50, 150, 0, 0));
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(50, 250, 0, 0));
  metrics()->OnGlicWindowClose(nullptr, display, gfx::Rect(150, 50, 0, 0));
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(150, 150, 0, 0));
  metrics()->OnGlicWindowClose(nullptr, display, gfx::Rect(150, 250, 0, 0));
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(250, 50, 0, 0));
  metrics()->OnGlicWindowClose(nullptr, display, gfx::Rect(250, 150, 0, 0));
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(250, 250, 0, 0));
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kTopLeft, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnClose",
                                       DisplayPosition::kCenterLeft, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kBottomLeft, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnClose",
                                       DisplayPosition::kTopCenter, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kCenterCenter, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnClose",
                                       DisplayPosition::kBottomCenter, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kTopRight, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnClose",
                                       DisplayPosition::kCenterRight, 1);
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kBottomRight, 1);
  // point is not within the work area bounds
  metrics()->OnGlicWindowShown(nullptr, display, gfx::Rect(-50, 50, 0, 0));
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kUnknown, 1);
  metrics()->OnGlicWindowClose(nullptr, display, gfx::Rect(50, -50, 0, 0));
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnClose",
                                       DisplayPosition::kUnknown, 1);
  // no display
  metrics()->OnGlicWindowShown(nullptr, std::nullopt, gfx::Rect(50, 50, 0, 0));
  histogram_tester().ExpectBucketCount("Glic.PositionOnDisplay.OnOpen",
                                       DisplayPosition::kUnknown, 2);
}

TEST_F(GlicMetricsTest, TabFocusStateReporting) {
  delegate()->tab_sharing_state = ActiveTabSharingState::kActiveTabIsShared;
  // Should not record samples on denying tab access or with the panel not
  // considered open.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, true);

  // Marks the panel as starting  to open; enables OnGlicWindowOpenAndReady to
  // record metrics.
  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  // Records a sample of *.OnPanelOpenAndReady.
  metrics()->OnGlicWindowOpenAndReady();

  delegate()->tab_sharing_state = ActiveTabSharingState::kCannotShareActiveTab;
  // Granting tab access records a sample of *.OnTabContextPermissionGranted.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, true);
  // Should not record a sample as the user is granting a different permission.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, true);

  delegate()->tab_sharing_state = ActiveTabSharingState::kNoTabCanBeShared;
  // Records a sample of *.OnUserInputSubmitted.
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);

  // Marks the panel as closed.
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  // Should not record samples on denying tab access or with the panel not
  // considered open.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, true);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnPanelOpenAndReady",
      ActiveTabSharingState::kActiveTabIsShared, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnTabContextPermissionGranted",
      ActiveTabSharingState::kCannotShareActiveTab, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnUserInputSubmitted",
      ActiveTabSharingState::kNoTabCanBeShared, 1);
}

TEST_F(GlicMetricsTest, FreToFirstQueryElapsedTimeReportedOnce) {
  metrics()->OnFreAccepted();
  task_environment().FastForwardBy(base::Milliseconds(100));
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester().ExpectTotalCount("Glic.FreToFirstQueryTime", 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 100, 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTimeMax24H", 100,
                                        1);
}

TEST_F(GlicMetricsTest, FreToFirstQueryElapsedTimeReportedOnlyOnce) {
  metrics()->OnFreAccepted();
  task_environment().FastForwardBy(base::Milliseconds(100));
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  // Second time should be ignored.
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester().ExpectTotalCount("Glic.FreToFirstQueryTime", 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 100, 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTimeMax24H", 100,
                                        1);
}

TEST_F(GlicMetricsTest, OnRecordUseCounter) {
  metrics()->OnRecordUseCounter(
      static_cast<uint16_t>(mojom::WebUseCounter::kMaxValue));
  metrics()->OnRecordUseCounter(
      static_cast<uint16_t>(mojom::WebUseCounter::kMaxValue) + 1);
  metrics()->OnRecordUseCounter(1001);

  histogram_tester().ExpectBucketCount("Glic.Api.UseCounter", 1000, 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Api.UseCounter",
      static_cast<uint16_t>(mojom::WebUseCounter::kMaxValue), 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Api.UseCounter",
      static_cast<uint16_t>(mojom::WebUseCounter::kMaxValue) + 1, 1);
  histogram_tester().ExpectTotalCount("Glic.Api.UseCounter", 3);
}

class GlicMetricsTrustFirstOnboardingTest : public GlicMetricsTest {
 public:
  void SetUp() override {
    GlicMetricsTest::SetUp();
    // Revert FRE status to NotStarted to simulate new user for this experiment.
    profile()->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kNotStarted));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kGlicTrustFirstOnboarding};
};

TEST_F(GlicMetricsTrustFirstOnboardingTest, ShownAndDismissed) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown.Onboarding"),
            1);

  // Closing without accept triggers "Dismissed".
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  EXPECT_EQ(
      user_action_tester().GetActionCount("Glic.Fre.Dismissed.Onboarding"), 1);
  histogram_tester().ExpectTotalCount("Glic.Fre.TotalTime.Dismissed.Onboarding",
                                      1);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, ShownAndAccepted) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown.Onboarding"),
            1);

  metrics()->OnTrustFirstOnboardingAccept();
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Accept"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Accept.Onboarding"),
            1);
  histogram_tester().ExpectTotalCount("Glic.Fre.TotalTime.Accepted.Onboarding",
                                      1);

  // Closing after accept should NOT trigger "Dismissed".
  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  EXPECT_EQ(
      user_action_tester().GetActionCount("Glic.Fre.Dismissed.Onboarding"), 0);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, NotShownIfConsented) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 0);

  metrics()->OnGlicWindowClose(nullptr, std::nullopt, gfx::Rect());
  EXPECT_EQ(
      user_action_tester().GetActionCount("Glic.Fre.Onboarding.Dismissed"), 0);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, FreToFirstQueryTimeRecorded) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnTrustFirstOnboardingAccept();

  task_environment().FastForwardBy(base::Seconds(1));
  metrics()->OnUserInputSubmitted(mojom::WebClientMode::kText);

  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 1000, 1);
}

}  // namespace
}  // namespace glic
