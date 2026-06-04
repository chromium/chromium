// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

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

namespace glic {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr int kBeyondShareTimeoutSeconds = 61;

class MockGlicHost : public Host {
 public:
  explicit MockGlicHost(Profile* profile)
      : Host(profile, nullptr, nullptr, nullptr) {}
  MOCK_METHOD(bool, IsWebClientConnected, (), (const, override));
};

class TestGlicShareImageHandler : public GlicShareImageHandler {
 public:
  explicit TestGlicShareImageHandler(GlicKeyedService& service)
      : GlicShareImageHandler(service) {}

  MOCK_METHOD(std::optional<bool>,
              IsClientReady,
              (tabs::TabInterface & tab),
              (override));
  MOCK_METHOD(void, DoPastePolicyCheck, (), (override));
};

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
    handler_ =
        std::make_unique<NiceMock<TestGlicShareImageHandler>>(*mock_service_);
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
        static_cast<int>(completed ? glic::prefs::FreStatus::kCompleted
                                   : glic::prefs::FreStatus::kNotStarted));
  }

  void SetTabHandle(tabs::TabHandle handle) { handler_->tab_handle_ = handle; }

  void SetOpenTime(base::TimeTicks timestamp) {
    handler_->glic_panel_open_time_ = timestamp;
  }

  void SetShareInProgress(bool in_progress) {
    handler_->is_share_in_progress_ = in_progress;
  }

  void PerformTaskWhenReady(base::OnceClosure callback = base::DoNothing()) {
    handler_->PerformTaskWhenReady(std::move(callback));
  }

  void ShareComplete(ShareImageResult result) {
    handler_->ShareComplete(result);
  }

  std::optional<bool> IsClientReady(tabs::TabInterface& tab) {
    return handler_->GlicShareImageHandler::IsClientReady(tab);
  }

  void CallDidFinishNavigation(content::NavigationHandle* handle) {
    handler_->DidFinishNavigation(handle);
  }

  void OnInvokeError(GlicInvokeError error) { handler_->OnInvokeError(error); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler enabler_;
  TestingProfileManager profile_manager_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<GlicEnabling> enabling_;
  raw_ptr<MockGlicKeyedService> mock_service_;
  std::unique_ptr<TestGlicShareImageHandler> handler_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::HistogramTester histogram_tester_;
};

TEST_F(GlicShareImageHandlerTest, TimeoutNoInstance) {
  tabs::MockTabInterface mock_tab;
  SetTabHandle(mock_tab.GetHandle());
  SetOpenTime(base::TimeTicks::Now());
  SetShareInProgress(true);

  EXPECT_CALL(*handler_, IsClientReady(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service_, GetInstanceForTab(_))
      .WillRepeatedly(Return(nullptr));
  PerformTaskWhenReady();
  task_environment_.FastForwardBy(base::Seconds(kBeyondShareTimeoutSeconds));

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedTimedOutNoInstance), 1);
}

TEST_F(GlicShareImageHandlerTest, TimeoutNoWebClient) {
  tabs::MockTabInterface mock_tab;
  SetTabHandle(mock_tab.GetHandle());
  SetOpenTime(base::TimeTicks::Now());
  SetShareInProgress(true);

  EXPECT_CALL(*handler_, IsClientReady(_)).WillRepeatedly(Return(false));

  InstanceId mock_id = InstanceId::Create(1u, 2);
  MockGlicInstance mock_instance;
  MockGlicHost mock_host(profile_);
  EXPECT_CALL(mock_instance, id()).WillRepeatedly(ReturnRef(mock_id));
  EXPECT_CALL(mock_instance, host()).WillRepeatedly(ReturnRef(mock_host));
  EXPECT_CALL(mock_host, IsWebClientConnected()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service_, GetInstanceForTab(_))
      .WillRepeatedly(Return(&mock_instance));
  PerformTaskWhenReady();
  task_environment_.FastForwardBy(base::Seconds(kBeyondShareTimeoutSeconds));

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(ShareImageResult::kFailedTimedOutNoWebClient), 1);
}

TEST_F(GlicShareImageHandlerTest, TimeoutDidNotCompleteOnboarding) {
  tabs::MockTabInterface mock_tab;
  SetTabHandle(mock_tab.GetHandle());
  SetOpenTime(base::TimeTicks::Now());
  SetShareInProgress(true);

  EXPECT_CALL(*handler_, IsClientReady(_)).WillRepeatedly(Return(false));
  SetFreCompletion(false);
  InstanceId mock_id = InstanceId::Create(1u, 2);
  MockGlicInstance mock_instance;
  MockGlicHost mock_host(profile_);
  EXPECT_CALL(mock_instance, id()).WillRepeatedly(ReturnRef(mock_id));
  EXPECT_CALL(mock_instance, host()).WillRepeatedly(ReturnRef(mock_host));
  EXPECT_CALL(mock_host, IsWebClientConnected()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_service_, GetInstanceForTab(_))
      .WillRepeatedly(Return(&mock_instance));
  PerformTaskWhenReady();
  task_environment_.FastForwardBy(base::Seconds(kBeyondShareTimeoutSeconds));

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(
          ShareImageResult::kFailedTimedOutDidNotCompleteOnboarding),
      1);
}

TEST_F(GlicShareImageHandlerTest, SawNavigationDidNotCompleteOnboarding) {
  tabs::MockTabInterface mock_tab;
  SetTabHandle(mock_tab.GetHandle());
  SetShareInProgress(true);
  SetFreCompletion(false);
  CallDidFinishNavigation(nullptr);

  histogram_tester_.ExpectBucketCount(
      "Glic.TabContext.ShareImageResult",
      static_cast<int>(
          ShareImageResult::kFailedSawNavigationDidNotCompleteOnboarding),
      1);
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
      static_cast<int>(
          ShareImageResult::kFailedTimedOutDidNotCompleteOnboarding),
      1);
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

}  // namespace glic
