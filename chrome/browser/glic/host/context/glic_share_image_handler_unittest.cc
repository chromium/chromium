// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

namespace {

using ::testing::NiceMock;

}  // namespace

class GlicShareImageHandlerTest : public testing::Test {
 public:
  GlicShareImageHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }

  ~GlicShareImageHandlerTest() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return std::make_unique<NiceMock<MockGlicKeyedService>>(
        context,
        IdentityManagerFactory::GetForProfile(
            Profile::FromBrowserContext(context)),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(&GlicShareImageHandlerTest::CreateService,
                             base::Unretained(this))});

    profile_ = profile_manager_.CreateTestingProfile("test_profile",
                                                     std::move(factories));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    mock_service_ = static_cast<MockGlicKeyedService*>(
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, true));

    enabling_ = GlicEnabling::CreateForTesting(
        profile_, profile_manager_.profile_attributes_storage());
    handler_ = std::make_unique<GlicShareImageHandler>(*mock_service_);
  }

  void TearDown() override {
    handler_.reset();
    enabling_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
  }

  void SetFreCompletion(bool completed) {
    profile_->GetPrefs()->SetInteger(
        glic::prefs::kGlicCompletedFre,
        std::to_underlying(completed ? glic::prefs::FreStatus::kCompleted
                                     : glic::prefs::FreStatus::kNotStarted));
  }

  void SetTabHandle(tabs::TabHandle handle) { handler_->tab_handle_ = handle; }

  void SetShareInProgress(bool in_progress) {
    handler_->is_share_in_progress_ = in_progress;
  }

  void SetCurrentInvocationInstance(base::WeakPtr<GlicInstance> instance) {
    handler_->current_invocation_instance_ = instance;
  }

  void CallReset() { handler_->Reset(); }

  bool IsShareInProgress() const { return handler_->is_share_in_progress_; }

  void CallDidFinishNavigation(content::NavigationHandle* handle) {
    handler_->DidFinishNavigation(handle);
  }

  void OnInvokeError(GlicInvokeError error) { handler_->OnInvokeError(error); }

  void OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus eligibility) {
    handler_->OnPageContextEligibilityChanged(eligibility);
  }

  void SetRenderFrameHostId(content::GlobalRenderFrameHostId id) {
    handler_->render_frame_host_id_ = id;
  }

  void OnReceivedImage(const std::vector<uint8_t>& thumbnail_data,
                       const gfx::Size& original_size,
                       const gfx::Size& downscaled_size,
                       const std::string& mime_type,
                       std::vector<lens::mojom::LatencyLogPtr> log_data) {
    handler_->OnReceivedImage(thumbnail_data, original_size, downscaled_size,
                              mime_type, std::move(log_data));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler enabler_;
  TestingProfileManager profile_manager_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<GlicEnabling> enabling_;
  raw_ptr<MockGlicKeyedService> mock_service_;
  std::unique_ptr<GlicShareImageHandler> handler_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::HistogramTester histogram_tester_;
};

TEST_F(GlicShareImageHandlerTest, SawNavigationDidNotCompleteOnboarding) {
  tabs::MockTabInterface mock_tab;
  SetTabHandle(mock_tab.GetHandle());
  SetShareInProgress(true);
  SetFreCompletion(false);
  CallDidFinishNavigation(nullptr);

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedSawNavigation), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorUnknown) {
  OnInvokeError(GlicInvokeError::kUnknown);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedUnknown), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInvalidConversationId) {
  OnInvokeError(GlicInvokeError::kInvalidConversationId);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedInvalidConversationId), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInvokeInProgress) {
  OnInvokeError(GlicInvokeError::kInvokeInProgress);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedInvokeInProgress), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInvalidConfiguration) {
  OnInvokeError(GlicInvokeError::kInvalidConfiguration);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedInvalidConfiguration), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorAdditionalContextNoClientFrame) {
  OnInvokeError(GlicInvokeError::kAdditionalContextNoClientFrame);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoClientFrame), 1);
}

TEST_F(GlicShareImageHandlerTest,
       OnInvokeErrorAdditionalContextNoClipboardMetadata) {
  OnInvokeError(GlicInvokeError::kAdditionalContextNoClipboardMetadata);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoClipboardMetadata), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorTimeoutConsented) {
  SetFreCompletion(true);
  OnInvokeError(GlicInvokeError::kTimeout);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedTimedOut), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorTimeoutNotConsented) {
  SetFreCompletion(false);
  OnInvokeError(GlicInvokeError::kTimeout);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedTimedOut), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInvalidTab) {
  OnInvokeError(GlicInvokeError::kInvalidTab);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoTab), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorTabClosed) {
  OnInvokeError(GlicInvokeError::kTabClosed);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoTab), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInstanceDestroyed) {
  OnInvokeError(GlicInvokeError::kInstanceDestroyed);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedLostInstance), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorInstanceNotFound) {
  OnInvokeError(GlicInvokeError::kInstanceNotFound);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedLostInstance), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorAdditionalContextSawNavigation) {
  OnInvokeError(GlicInvokeError::kAdditionalContextSawNavigation);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedSawNavigation), 1);
}

TEST_F(GlicShareImageHandlerTest,
       OnInvokeErrorAdditionalContextFailedCopyPolicy) {
  OnInvokeError(GlicInvokeError::kAdditionalContextFailedCopyPolicy);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedClipboardCopyPolicy), 1);
}

TEST_F(GlicShareImageHandlerTest,
       OnInvokeErrorAdditionalContextFailedPastePolicy) {
  OnInvokeError(GlicInvokeError::kAdditionalContextFailedPastePolicy);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedClipboardPastePolicy), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorAdditionalContextNoSourceFrame) {
  OnInvokeError(GlicInvokeError::kAdditionalContextNoSourceFrame);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoFrame), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorCancelled) {
  OnInvokeError(GlicInvokeError::kCancelled);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedCancelled), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorProfileNotEnabled) {
  OnInvokeError(GlicInvokeError::kProfileNotEnabled);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedProfileNotEnabled), 1);
}

TEST_F(GlicShareImageHandlerTest, OnInvokeErrorSuperseded) {
  OnInvokeError(GlicInvokeError::kSuperseded);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedSuperseded), 1);
}

TEST_F(GlicShareImageHandlerTest,
       PageContextEligibilityChangedToIneligibleFails) {
  SetShareInProgress(true);
  OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoTabContext), 1);
}

TEST_F(GlicShareImageHandlerTest, PageContextEligibilityChangedToUnknownFails) {
  SetShareInProgress(true);
  OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kUnknown);
  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedNoTabContext), 1);
}

TEST_F(GlicShareImageHandlerTest,
       PageContextEligibilityChangedToEligibleDoesNotFail) {
  SetShareInProgress(true);
  OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
  histogram_tester_.ExpectTotalCount("Glic.TabContext.ShareImageResult", 0);
}

TEST_F(GlicShareImageHandlerTest, OnReceivedImageUsesNewConversationByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kGlicShareImageNoNewConversation);

  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile_, content::SiteInstance::Create(profile_));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  SetTabHandle(mock_tab.GetHandle());
  SetShareInProgress(true);
  SetRenderFrameHostId(web_contents->GetPrimaryMainFrame()->GetGlobalId());

  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([](GlicInvokeOptions options) {
        EXPECT_TRUE(std::holds_alternative<NewConversation>(
            options.target.conversation));
        return base::WeakPtr<GlicInstance>();
      });

  std::vector<uint8_t> thumbnail_data = {1, 2, 3};
  OnReceivedImage(thumbnail_data, gfx::Size(10, 10), gfx::Size(10, 10),
                  "image/png", {});
}

TEST_F(GlicShareImageHandlerTest, OnReceivedImageWithNoNewConversationFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicShareImageNoNewConversation);

  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile_, content::SiteInstance::Create(profile_));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  SetTabHandle(mock_tab.GetHandle());
  SetShareInProgress(true);
  SetRenderFrameHostId(web_contents->GetPrimaryMainFrame()->GetGlobalId());

  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([](GlicInvokeOptions options) {
        EXPECT_TRUE(std::holds_alternative<DefaultConversation>(
            options.target.conversation));
        return base::WeakPtr<GlicInstance>();
      });

  std::vector<uint8_t> thumbnail_data = {1, 2, 3};
  OnReceivedImage(thumbnail_data, gfx::Size(10, 10), gfx::Size(10, 10),
                  "image/png", {});
}

TEST_F(GlicShareImageHandlerTest, ResetCancelsActiveInvocation) {
  MockGlicInstance mock_instance;
  EXPECT_CALL(mock_instance, CancelInvoke()).Times(1);

  SetShareInProgress(true);
  SetCurrentInvocationInstance(mock_instance.GetWeakPtr());

  CallReset();
  EXPECT_FALSE(IsShareInProgress());
}

class FakePageContextEligibilityHelper
    : public tabs::PageContextEligibilityHelper {
 public:
  explicit FakePageContextEligibilityHelper(tabs::TabInterface& tab)
      : tabs::PageContextEligibilityHelper(tab) {}
  optimization_guide::PageContextEligibilityStatus IsPageContextEligible()
      const override {
    return optimization_guide::PageContextEligibilityStatus::kEligible;
  }
};

TEST_F(GlicShareImageHandlerTest, ResetWithoutActiveInvocationDoesNotCancel) {
  MockGlicInstance mock_instance;
  EXPECT_CALL(mock_instance, CancelInvoke()).Times(0);

  SetShareInProgress(false);
  SetCurrentInvocationInstance(mock_instance.GetWeakPtr());

  CallReset();
}

TEST_F(GlicShareImageHandlerTest,
       ShareContextImageReplacesInProgressShareCancelsActiveInvocation) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("http://example.com/page"));
  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost unowned_user_data_host;
  EXPECT_CALL(mock_tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents.get()));
  EXPECT_CALL(mock_tab, GetTabHandle()).WillRepeatedly(testing::Return(12345));
  EXPECT_CALL(mock_tab, GetUnownedUserDataHost())
      .WillRepeatedly(testing::ReturnRef(unowned_user_data_host));

  FakePageContextEligibilityHelper fake_helper(mock_tab);

  MockGlicInstance mock_instance;
  EXPECT_CALL(mock_instance, CancelInvoke()).Times(1);

  SetShareInProgress(true);
  SetCurrentInvocationInstance(mock_instance.GetWeakPtr());

  handler_->ShareContextImage(&mock_tab, web_contents->GetPrimaryMainFrame(),
                              GURL("http://example.com/image.png"));

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedReplacedByNewShare), 1);
  EXPECT_TRUE(IsShareInProgress());
}

TEST_F(GlicShareImageHandlerTest,
       ShareContextImageReplacesInProgressShareWithoutActiveInvocation) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("http://example.com/page"));
  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost unowned_user_data_host;
  EXPECT_CALL(mock_tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents.get()));
  EXPECT_CALL(mock_tab, GetTabHandle()).WillRepeatedly(testing::Return(12345));
  EXPECT_CALL(mock_tab, GetUnownedUserDataHost())
      .WillRepeatedly(testing::ReturnRef(unowned_user_data_host));

  FakePageContextEligibilityHelper fake_helper(mock_tab);

  SetShareInProgress(true);

  handler_->ShareContextImage(&mock_tab, web_contents->GetPrimaryMainFrame(),
                              GURL("http://example.com/image.png"));

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedReplacedByNewShare), 1);
  EXPECT_TRUE(IsShareInProgress());
}

}  // namespace glic
