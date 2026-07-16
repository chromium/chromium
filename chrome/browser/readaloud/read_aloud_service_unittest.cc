// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/readaloud/read_aloud_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

namespace {

class MockDelegate : public ReadAloudService::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              OnMetadataAvailable,
              (std::string_view title, std::string_view publisher),
              (override));
  MOCK_METHOD(void,
              OnPlaybackProgressUpdated,
              (base::TimeDelta elapsed, base::TimeDelta duration),
              (override));
  MOCK_METHOD(void,
              OnPlaybackStateChanged,
              (ReadAloudService::PlaybackState playback_state),
              (override));
  MOCK_METHOD(void,
              OnVoicesAvailable,
              (const std::vector<ReadAloudService::Voice>& voices,
               std::string_view selected_voice_id),
              (override));
  MOCK_METHOD(void,
              OnWordHighlightUpdated,
              (int absolute_start_index, int absolute_end_index),
              (override));
  MOCK_METHOD(void, OnHighlightingSupported, (bool supported), (override));
  MOCK_METHOD(void, OnFallbackEngaged, (), (override));
  MOCK_METHOD(void,
              OnPlaybackError,
              (std::string_view error_message),
              (override));
  MOCK_METHOD(void,
              OnVoicePreviewPlaybackStateChanged,
              (std::string_view voice_id,
               ReadAloudService::PlaybackState playback_state),
              (override));
  MOCK_METHOD(void,
              OnReadabilityResult,
              (const GURL& url, bool is_readable),
              (override));
  MOCK_METHOD(void, OnNativeDestroyed, (), (override));
};

class MockDomDistillerService
    : public dom_distiller::DomDistillerContextKeyedService {
 public:
  MockDomDistillerService()
      : DomDistillerContextKeyedService(nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        {}) {}
  MOCK_METHOD(std::unique_ptr<dom_distiller::ViewerHandle>,
              ViewUrlIgnoreCache,
              (dom_distiller::ViewRequestDelegate*,
               std::unique_ptr<dom_distiller::DistillerPage>,
               const GURL&),
              (override));
  MOCK_METHOD(std::unique_ptr<dom_distiller::DistillerPage>,
              CreateDefaultDistillerPageWithHandle,
              (std::unique_ptr<dom_distiller::SourcePageHandle>),
              (override));
};

std::unique_ptr<KeyedService> BuildMockDomDistillerService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockDomDistillerService>>();
}

std::unique_ptr<KeyedService> BuildMockMediaRouter(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<media_router::MockMediaRouter>>();
}

}  // namespace

class ReadAloudServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kReadAloudNative);

    dom_distiller::DomDistillerServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockDomDistillerService));

    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockMediaRouter));
  }

  MockDomDistillerService* mock_distiller_service() {
    return static_cast<MockDomDistillerService*>(
        dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
            profile()));
  }

  ReadAloudService* service() {
    return ReadAloudServiceFactory::GetForProfile(profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAloudServiceTest, DistillNullWebContents) {
  // Should be a completely safe no-op.
  service()->DistillPage(nullptr);
  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReady) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  base::HistogramTester histograms;

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  dom_distiller::ViewRequestDelegate* delegate_ptr = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* delegate,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        delegate_ptr = delegate;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  service()->DistillPage(web_contents());

  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());
  ASSERT_NE(nullptr, delegate_ptr);

  // Simulate DomDistiller finishing distillation with success (contains pages).
  dom_distiller::DistilledArticleProto proto;
  proto.add_pages();
  delegate_ptr->OnArticleReady(&proto);

  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
  histograms.ExpectTotalCount("ReadAloud.Distillation.Duration", 1);
  histograms.ExpectUniqueSample("ReadAloud.Distillation.Success", true, 1);
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleFailure) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  base::HistogramTester histograms;

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  dom_distiller::ViewRequestDelegate* delegate_ptr = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* delegate,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        delegate_ptr = delegate;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  service()->DistillPage(web_contents());

  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());
  ASSERT_NE(nullptr, delegate_ptr);

  // Simulate DomDistiller finishing distillation with failure (no pages).
  dom_distiller::DistilledArticleProto proto;
  delegate_ptr->OnDistillationFailed(
      dom_distiller::DistillationParseResult::kContentTooShort);
  delegate_ptr->OnArticleReady(&proto);

  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
  histograms.ExpectTotalCount("ReadAloud.Distillation.Duration", 1);
  histograms.ExpectUniqueSample("ReadAloud.Distillation.Success", false, 1);
  histograms.ExpectUniqueSample(
      "ReadAloud.Distillation.FailureReason",
      dom_distiller::DistillationParseResult::kContentTooShort, 1);
}

TEST_F(ReadAloudServiceTest, OnArticleUpdated) {
  dom_distiller::ArticleDistillationUpdate update({}, false, false);
  // Should be a completely safe no-op.
  service()->OnArticleUpdated(update);
}

TEST_F(ReadAloudServiceTest, ShutdownClearsHandle) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing()))));

  service()->Play(web_contents());
  service()->DistillPage(web_contents());
  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());

  service()->Shutdown();
  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
  EXPECT_EQ(nullptr, service()->web_contents());
}

TEST_F(ReadAloudServiceTest, StopDetachesWebContentsObserver) {
  service()->Play(web_contents());
  EXPECT_EQ(web_contents(), service()->web_contents());

  service()->Stop();
  EXPECT_EQ(nullptr, service()->web_contents());
}

TEST_F(ReadAloudServiceTest, PlayNullWebContents) {
  // Should safely return early without crashing or modifying web_contents.
  service()->Play(nullptr);
  EXPECT_EQ(nullptr, service()->web_contents());
}

TEST_F(ReadAloudServiceTest, SetDelegateAndShutdownLifecycle) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();

  // Initially, there is no delegate.
  EXPECT_EQ(nullptr, service()->delegate());

  // Registering the delegate should succeed and be accessible.
  service()->SetDelegate(std::move(delegate));
  EXPECT_EQ(delegate_ptr, service()->delegate());

  // Shutdown should trigger OnNativeDestroyed() exactly once and clear the
  // delegate.
  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
  service()->Shutdown();
  EXPECT_EQ(nullptr, service()->delegate());
}

TEST_F(ReadAloudServiceTest, PrimaryPageChangedStopsAndDetachesObserver) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing()))));

  service()->Play(web_contents());
  service()->DistillPage(web_contents());
  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kStopped))
      .Times(1);

  // Navigating to a new URL triggers PrimaryPageChanged().
  NavigateAndCommit(GURL("https://www.example.com/other"));

  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
  EXPECT_EQ(nullptr, service()->web_contents());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       WebContentsDestroyedStopsAndDetachesObserver) {
  std::unique_ptr<content::WebContents> test_contents =
      CreateTestWebContents();
  service()->Play(test_contents.get());
  EXPECT_EQ(test_contents.get(), service()->web_contents());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kStopped))
      .Times(1);

  // Deleting the observed WebContents triggers WebContentsDestroyed().
  test_contents.reset();

  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
  EXPECT_EQ(nullptr, service()->web_contents());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

}  // namespace readaloud
