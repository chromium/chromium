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
#include "mojo/public/mojom/base/work_in_progress.mojom.h"
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

class FakePlaybackController
    : public read_aloud::mojom::ReadAloudPlaybackController {
 public:
  FakePlaybackController() = default;
  explicit FakePlaybackController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  ~FakePlaybackController() override = default;

  void Bind(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

  void Reset() { receiver_.reset(); }

  void InitializeAudio(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe) override {}

  void SetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments) override {
    received_segments_ = std::move(segments);
  }

  void Play() override {}
  void Pause() override {}
  void SeekToWord(uint32_t segment_index, uint32_t character_offset) override {}
  void SeekToTime(base::TimeDelta position) override {}
  void SetVoice(const std::string& voice_id) override {}
  void SetPlaybackRate(float rate) override {}
  void FlushBuffers() override {}

  const std::vector<read_aloud::mojom::TextSegmentPtr>& received_segments() const {
    return received_segments_;
  }

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController> receiver_{this};
  std::vector<read_aloud::mojom::TextSegmentPtr> received_segments_;
};

}  // namespace

class ReadAloudServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAloudNative, mojo_base::mojom::kMojomWorkInProgress},
        {});

    dom_distiller::DomDistillerServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockDomDistillerService));

    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockMediaRouter));

    ReadAloudServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            [](ReadAloudServiceTest* test, content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<ReadAloudService>(
                  Profile::FromBrowserContext(context),
                  base::BindRepeating(&ReadAloudServiceTest::BindController,
                                      base::Unretained(test)));
            },
            this));
  }

  MockDomDistillerService* mock_distiller_service() {
    return static_cast<MockDomDistillerService*>(
        dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
            profile()));
  }

  ReadAloudService* service() {
    return ReadAloudServiceFactory::GetForProfile(profile());
  }

  dom_distiller::ViewerHandle* GetViewerHandle() {
    return service()->GetViewerHandleForTesting();
  }

  void SetFakeController(
      std::unique_ptr<FakePlaybackController> controller) {
    fake_controller_ = std::move(controller);
  }

  FakePlaybackController* fake_controller() {
    return fake_controller_.get();
  }

  void BindController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver) {
    if (fake_controller_) {
      fake_controller_->Bind(std::move(receiver));
    }
  }

 private:
  std::unique_ptr<FakePlaybackController> fake_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAloudServiceTest, DistillNullWebContents) {
  // Should be a completely safe no-op.
  service()->DistillPage(nullptr);
  EXPECT_EQ(GetViewerHandle(), nullptr);
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReady) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  base::HistogramTester histograms;

  SetFakeController(std::make_unique<FakePlaybackController>());

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

  EXPECT_NE(GetViewerHandle(), nullptr);
  ASSERT_NE(delegate_ptr, nullptr);

  // Simulate DomDistiller finishing distillation with multi-page article.
  dom_distiller::DistilledArticleProto proto;
  dom_distiller::DistilledPageProto* page1 = proto.add_pages();
  page1->set_html("First page content");
  dom_distiller::DistilledPageProto* page2 = proto.add_pages();
  page2->set_html("Second page content");

  delegate_ptr->OnArticleReady(&proto);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetViewerHandle(), nullptr);
  histograms.ExpectTotalCount("ReadAloud.Distillation.Duration", 1);
  histograms.ExpectUniqueSample("ReadAloud.Distillation.Success", true, 1);

  const std::vector<read_aloud::mojom::TextSegmentPtr>& segments =
      fake_controller()->received_segments();
  ASSERT_EQ(2u, segments.size());
  EXPECT_EQ(0u, segments[0]->segment_index);
  EXPECT_EQ(u"First page content", segments[0]->text);
  EXPECT_EQ(1u, segments[1]->segment_index);
  EXPECT_EQ(u"Second page content", segments[1]->text);
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReady_EmptyPageHtml) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  SetFakeController(std::make_unique<FakePlaybackController>());

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
  ASSERT_NE(delegate_ptr, nullptr);

  // Distilled article where second page is empty string.
  dom_distiller::DistilledArticleProto proto;
  dom_distiller::DistilledPageProto* page1 = proto.add_pages();
  page1->set_html("Page 1 text");
  dom_distiller::DistilledPageProto* page2 = proto.add_pages();
  page2->set_html("");

  delegate_ptr->OnArticleReady(&proto);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetViewerHandle(), nullptr);

  const std::vector<read_aloud::mojom::TextSegmentPtr>& segments =
      fake_controller()->received_segments();
  ASSERT_EQ(2u, segments.size());
  EXPECT_EQ(0u, segments[0]->segment_index);
  EXPECT_EQ(u"Page 1 text", segments[0]->text);
  EXPECT_EQ(1u, segments[1]->segment_index);
  EXPECT_EQ(u"", segments[1]->text);
}

TEST_F(ReadAloudServiceTest, UtilityDisconnectTriggersErrorAndStop) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr_mock = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  EXPECT_CALL(*delegate_ptr_mock, OnPlaybackError("Utility process disconnected")).Times(1);
  EXPECT_CALL(*delegate_ptr_mock, OnNativeDestroyed()).Times(1);

  // Force service connection to bind fake controller:
  service()->Initialize();

  // Simulating utility process crash by destroying receiver.
  fake_controller()->Reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleFailure) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  base::HistogramTester histograms;

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr_mock = delegate.get();
  service()->SetDelegate(std::move(delegate));

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

  EXPECT_NE(nullptr, GetViewerHandle());
  ASSERT_NE(nullptr, delegate_ptr);

  EXPECT_CALL(*delegate_ptr_mock, OnPlaybackError("Distillation failed")).Times(1);
  EXPECT_CALL(*delegate_ptr_mock, OnNativeDestroyed()).Times(1);

  // Simulate DomDistiller finishing distillation with failure (no pages).
  dom_distiller::DistilledArticleProto proto;
  delegate_ptr->OnDistillationFailed(
      dom_distiller::DistillationParseResult::kContentTooShort);
  delegate_ptr->OnArticleReady(&proto);

  EXPECT_EQ(nullptr, GetViewerHandle());
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
  EXPECT_NE(nullptr, GetViewerHandle());

  service()->Shutdown();
  EXPECT_EQ(nullptr, GetViewerHandle());
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
  EXPECT_NE(nullptr, GetViewerHandle());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kStopped))
      .Times(1);

  // Navigating to a new URL triggers PrimaryPageChanged().
  NavigateAndCommit(GURL("https://www.example.com/other"));

  EXPECT_EQ(nullptr, GetViewerHandle());
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

  EXPECT_EQ(nullptr, GetViewerHandle());
  EXPECT_EQ(nullptr, service()->web_contents());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, VoicePreviewDispatchesPlayingAndStoppedStates) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  "msf00006", ReadAloudService::PlaybackState::kPlaying))
      .Times(1);
  service()->PreviewVoice("msf00006");

  EXPECT_CALL(*delegate_ptr, OnVoicePreviewPlaybackStateChanged(
                                 "", ReadAloudService::PlaybackState::kStopped))
      .Times(1);
  service()->StopVoicePreview();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, PreviewVoicePausesActivePlayback) {
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  service()->Play(test_contents.get());
  EXPECT_EQ(test_contents.get(), service()->web_contents());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  // PreviewVoice should pause active article playback and start the requested
  // voice preview.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kPaused))
      .Times(1);
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  "msf00006", ReadAloudService::PlaybackState::kPlaying))
      .Times(1);
  service()->PreviewVoice("msf00006");

  // Stopping playback returns article state to stopped before teardown.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, PlayResumesPlaybackAfterVoicePreview) {
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  service()->Play(test_contents.get());
  service()->PreviewVoice("msf00006");

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  // Starting article playback again resumes article playback.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kPlaying))
      .Times(1);
  service()->Play(test_contents.get());

  // Explicitly stopping playback returns article state to stopped before
  // teardown.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(ReadAloudService::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

}  // namespace readaloud
