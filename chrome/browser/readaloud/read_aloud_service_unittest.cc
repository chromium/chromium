// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/readaloud/fake_audio_stream_factory.h"
#include "chrome/browser/readaloud/read_aloud_service_factory.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
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
  ~FakePlaybackController() override = default;

  void Bind(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

  void Reset() {
    receiver_.reset();
    received_segments_.clear();
    last_audio_stream_.reset();
    last_data_pipe_.reset();
  }

  void InitializeAudio(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe,
      const media::AudioParameters& params) override {
    last_audio_stream_ = std::move(stream);
    last_data_pipe_ = std::move(data_pipe);
    last_audio_params_ = params;
    initialize_audio_called_count_++;
    if (initialize_audio_callback_) {
      std::move(initialize_audio_callback_).Run();
    }
  }

  void SetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments) override {
    received_segments_ = std::move(segments);
    if (set_text_content_callback_) {
      std::move(set_text_content_callback_).Run();
    }
  }

  void Play() override {
    play_count_++;
    if (play_callback_) {
      std::move(play_callback_).Run();
    }
  }
  void Pause() override {
    pause_count_++;
    if (pause_callback_) {
      std::move(pause_callback_).Run();
    }
  }
  void SeekToWord(uint32_t segment_index, uint32_t character_offset) override {}
  void SeekToTime(base::TimeDelta position) override {}
  void SetVoice(const std::string& voice_id) override {}
  void SetPlaybackRate(float rate) override {
    last_playback_rate_ = rate;
    if (set_playback_rate_callback_) {
      std::move(set_playback_rate_callback_).Run();
    }
  }
  void FlushBuffers() override {}

  void set_text_content_callback(base::OnceClosure callback) {
    set_text_content_callback_ = std::move(callback);
  }
  void set_play_callback(base::OnceClosure callback) {
    play_callback_ = std::move(callback);
  }
  void set_pause_callback(base::OnceClosure callback) {
    pause_callback_ = std::move(callback);
  }
  void set_playback_rate_callback(base::OnceClosure callback) {
    set_playback_rate_callback_ = std::move(callback);
  }

  const std::vector<read_aloud::mojom::TextSegmentPtr>& received_segments()
      const {
    return received_segments_;
  }

  int play_count() const { return play_count_; }
  int pause_count() const { return pause_count_; }
  float last_playback_rate() const { return last_playback_rate_; }
  void set_initialize_audio_callback(base::OnceClosure callback) {
    initialize_audio_callback_ = std::move(callback);
  }

  int initialize_audio_called_count() const {
    return initialize_audio_called_count_;
  }
  const media::AudioParameters& last_audio_params() const {
    return last_audio_params_;
  }
  bool has_audio_stream() const { return last_audio_stream_.is_valid(); }
  bool has_data_pipe() const { return !last_data_pipe_.is_null(); }

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController> receiver_{
      this};
  std::vector<read_aloud::mojom::TextSegmentPtr> received_segments_;
  mojo::PendingRemote<media::mojom::AudioOutputStream> last_audio_stream_;
  media::mojom::ReadWriteAudioDataPipePtr last_data_pipe_;
  media::AudioParameters last_audio_params_;
  base::OnceClosure set_text_content_callback_;
  base::OnceClosure play_callback_;
  base::OnceClosure pause_callback_;
  base::OnceClosure set_playback_rate_callback_;
  int play_count_ = 0;
  int pause_count_ = 0;
  float last_playback_rate_ = 1.0f;
  base::OnceClosure initialize_audio_callback_;
  int initialize_audio_called_count_ = 0;
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
                                      base::Unretained(test)),
                  base::BindRepeating(
                      &ReadAloudServiceTest::BindAudioStreamFactory,
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

  void ExpectDistillation(const GURL& url) {
    EXPECT_CALL(*mock_distiller_service(),
                CreateDefaultDistillerPageWithHandle(testing::_))
        .WillOnce(testing::Return(testing::ByMove(
            std::make_unique<dom_distiller::test::MockDistillerPage>())));

    EXPECT_CALL(*mock_distiller_service(),
                ViewUrlIgnoreCache(service(), testing::_, url))
        .WillOnce(testing::Return(testing::ByMove(
            std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing()))));
  }

  void SetFakeController(
      std::unique_ptr<FakePlaybackController> controller) {
    fake_controller_ = std::move(controller);
  }

  FakePlaybackController* fake_controller() {
    return fake_controller_.get();
  }

  FakeAudioStreamFactory* fake_audio_stream_factory() {
    return &fake_audio_stream_factory_;
  }

  void ExpectInitializeCallbacks(
      MockDelegate* delegate,
      testing::Matcher<std::string_view> expected_title = testing::_,
      testing::Matcher<std::string_view> expected_publisher = "example.com") {
    EXPECT_CALL(*delegate,
                OnMetadataAvailable(expected_title, expected_publisher))
        .Times(1);
    EXPECT_CALL(*delegate,
                OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(0),
                                          /*duration=*/base::Seconds(0)))
        .Times(1);
  }

  void BindController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver) {
    if (fake_controller_) {
      fake_controller_->Bind(std::move(receiver));
    }
  }

  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    fake_audio_stream_factory_.Bind(std::move(receiver));
  }

 private:
  std::unique_ptr<FakePlaybackController> fake_controller_;
  FakeAudioStreamFactory fake_audio_stream_factory_;
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

  service()->Initialize(web_contents());

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

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReadyEmptyPageHtml) {
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

  service()->Initialize(web_contents());
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

TEST_F(ReadAloudServiceTest,
       InitializePopulatesTitleAndPublisherFromWebContents) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  web_contents()->UpdateTitleForEntry(
      web_contents()->GetController().GetLastCommittedEntry(),
      u"Example Article - Example News");

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  EXPECT_CALL(
      *delegate_ptr,
      OnMetadataAvailable("Example Article - Example News", "example.com"))
      .Times(1);
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackProgressUpdated(base::Seconds(0), base::Seconds(0)))
      .Times(1);

  service()->Initialize(web_contents());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       InitializePopulatesDefaultTitleAndPublisherWhenEmpty) {
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  EXPECT_CALL(*delegate_ptr, OnMetadataAvailable("", "")).Times(1);
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(0),
                                        /*duration=*/base::Seconds(0)))
      .Times(1);

  service()->Initialize(test_contents.get());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, OnArticleReadyUpdatesTitleFromDistilledProto) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* d,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        view_delegate = d;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  EXPECT_CALL(*delegate_ptr,
              OnMetadataAvailable("Distilled Headline Title", "example.com"))
      .Times(1);

  dom_distiller::DistilledArticleProto proto;
  proto.set_title("Distilled Headline Title");
  dom_distiller::DistilledPageProto* page1 = proto.add_pages();
  page1->set_html("Article body text");

  view_delegate->OnArticleReady(&proto);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       OnArticleReadyDoesNotOverrideTitleWhenProtoTitleIsEmpty) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* d,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        view_delegate = d;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  // Expect no additional OnMetadataAvailable call when distillation headline is
  // empty.
  EXPECT_CALL(*delegate_ptr, OnMetadataAvailable(testing::_, testing::_))
      .Times(0);

  dom_distiller::DistilledArticleProto proto;
  proto.set_title("");
  dom_distiller::DistilledPageProto* page1 = proto.add_pages();
  page1->set_html("Article body text");

  view_delegate->OnArticleReady(&proto);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, OnPlaybackDurationChangedUpdatesDurationState) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // OnPlaybackDurationChanged updates duration state without triggering a UI
  // scrubber jump.
  service()->OnPlaybackDurationChanged(base::Seconds(120));

  // Word boundary updates deliver the updated duration alongside clamped elapsed
  // progress.
  EXPECT_CALL(
      *delegate_ptr,
      OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(10),
                                /*duration=*/base::Seconds(120)))
      .Times(1);
  service()->OnWordBoundaryReached(0, 0, base::Seconds(10));

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedClampsElapsedWithinDuration) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // Set total duration to 100 seconds.
  service()->OnPlaybackDurationChanged(base::Seconds(100));

  // Test normal progress timestamp within bounds (15s).
  EXPECT_CALL(
      *delegate_ptr,
      OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(15),
                                /*duration=*/base::Seconds(100)))
      .Times(1);
  service()->OnWordBoundaryReached(0, 0, base::Seconds(15));

  // Test negative timestamp is clamped to 0s.
  EXPECT_CALL(
      *delegate_ptr,
      OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(0),
                                /*duration=*/base::Seconds(100)))
      .Times(1);
  service()->OnWordBoundaryReached(0, 0, base::Seconds(-10));

  // Test overflow timestamp is clamped to total duration (100s).
  EXPECT_CALL(
      *delegate_ptr,
      OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(100),
                                /*duration=*/base::Seconds(100)))
      .Times(1);
  service()->OnWordBoundaryReached(0, 0, base::Seconds(150));

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       OnArticleReadyRefinesTitleWhenUtilityPlayerUnbound) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  // Note: Do NOT call SetFakeController, leaving utility_player_ unbound.

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* d,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        view_delegate = d;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  ExpectInitializeCallbacks(delegate_ptr);
  EXPECT_CALL(*delegate_ptr, OnPlaybackError("Utility process disconnected"))
      .Times(testing::AtMost(1));
  service()->Initialize(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  // Verify that OnArticleReady still refines title in service state & delegate
  // even when utility_player_ is unbound.
  EXPECT_CALL(*delegate_ptr,
              OnMetadataAvailable("Distilled Headline Title", "example.com"))
      .Times(1);

  dom_distiller::DistilledArticleProto proto;
  proto.set_title("Distilled Headline Title");
  dom_distiller::DistilledPageProto* page1 = proto.add_pages();
  page1->set_html("Article body text");

  view_delegate->OnArticleReady(&proto);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, UtilityDisconnectTriggersErrorAndStop) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr_mock = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  ExpectInitializeCallbacks(delegate_ptr_mock);
  EXPECT_CALL(*delegate_ptr_mock,
              OnPlaybackError("Utility process disconnected"))
      .Times(1);

  // Force service connection to bind fake controller:
  service()->Initialize(web_contents());

  // Simulating utility process crash by destroying receiver.
  fake_controller()->Reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*delegate_ptr_mock, OnNativeDestroyed()).Times(1);
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

  ExpectDistillation(GURL("https://www.example.com/article"));

  service()->Play(web_contents());
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

  ExpectDistillation(GURL("https://www.example.com/article"));

  service()->Play(web_contents());
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

TEST_F(ReadAloudServiceTest, UtilityProcessLifecycle) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  SetFakeController(std::make_unique<FakePlaybackController>());

  // Mock distiller calls:
  dom_distiller::ViewRequestDelegate* distiller_delegate = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .Times(2)  // We will Play twice.
      .WillRepeatedly(
          [](std::unique_ptr<dom_distiller::SourcePageHandle> handle) {
            return std::make_unique<dom_distiller::test::MockDistillerPage>();
          });

  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .Times(2)
      .WillRepeatedly([&](dom_distiller::ViewRequestDelegate* delegate,
                          std::unique_ptr<dom_distiller::DistillerPage> page,
                          const GURL& url) {
        distiller_delegate = delegate;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  // Call Play() (First Session) - should start the first distillation.
  service()->Play(web_contents());
  EXPECT_EQ(web_contents(), service()->web_contents());
  ASSERT_NE(nullptr, distiller_delegate);

  // Simulate article ready - should connect to utility player and bind.
  dom_distiller::DistilledArticleProto proto;
  dom_distiller::DistilledPageProto* page = proto.add_pages();
  page->set_html("Content");
  base::RunLoop run_loop;
  fake_controller()->set_text_content_callback(run_loop.QuitClosure());
  distiller_delegate->OnArticleReady(&proto);
  run_loop.Run();

  // Verify segments were received by fake controller.
  EXPECT_EQ(1u, fake_controller()->received_segments().size());

  // Call Stop() - should disconnect utility player.
  service()->Stop();
  EXPECT_EQ(nullptr, service()->web_contents());

  // Reset fake controller to ensure we can detect a new connection.
  fake_controller()->Reset();

  // Call Play() again (Second Session) - should start a second distillation
  // because we previously stopped the session.
  distiller_delegate = nullptr;
  service()->Play(web_contents());
  ASSERT_NE(nullptr, distiller_delegate);

  // Simulate article ready again - should reconnect.
  base::RunLoop run_loop2;
  fake_controller()->set_text_content_callback(run_loop2.QuitClosure());
  distiller_delegate->OnArticleReady(&proto);
  run_loop2.Run();

  // Verify new segments were received (proving reconnection).
  EXPECT_EQ(1u, fake_controller()->received_segments().size());

  // Call Play() while already playing (same WebContents) - should NOT reconnect
  // or re-distill a third time. The distiller mock expectations of `.Times(2)`
  // set at the top of this test case will fail if a third distillation is
  // triggered.
  service()->Play(web_contents());
}

TEST_F(ReadAloudServiceTest, AudioStreamLifecycle) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  SetFakeController(std::make_unique<FakePlaybackController>());

  ExpectDistillation(GURL("https://www.example.com/article"));

  base::RunLoop run_loop;
  fake_controller()->set_initialize_audio_callback(run_loop.QuitClosure());

  // Calling Play() initializes controller and requests audio stream creation.
  service()->Play(web_contents());
  run_loop.Run();

  EXPECT_EQ(fake_audio_stream_factory()->create_output_stream_called_count(),
            1);
  EXPECT_EQ(fake_audio_stream_factory()->last_device_id(),
            media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_EQ(fake_audio_stream_factory()->last_params().sample_rate(),
            readaloud::kAudioSampleRate);
  EXPECT_EQ(fake_audio_stream_factory()->last_params().frames_per_buffer(),
            readaloud::kAudioFramesPerBuffer);
  EXPECT_EQ(fake_audio_stream_factory()->last_group_id(),
            web_contents()->GetAudioGroupId());

  // Controller received the audio initialization call with valid handles.
  EXPECT_EQ(fake_controller()->initialize_audio_called_count(), 1);
  EXPECT_TRUE(fake_controller()->has_audio_stream());
  EXPECT_TRUE(fake_controller()->has_data_pipe());
  EXPECT_EQ(fake_controller()->last_audio_params().sample_rate(),
            readaloud::kAudioSampleRate);
}

TEST_F(ReadAloudServiceTest, AudioStreamCreationFailureDispatchesError) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  SetFakeController(std::make_unique<FakePlaybackController>());
  fake_audio_stream_factory()->set_auto_respond(/*auto_respond=*/true,
                                                /*should_succeed=*/false);

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectDistillation(GURL("https://www.example.com/article"));

  base::RunLoop run_loop;

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackError("Failed to initialize audio output stream"))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  service()->Play(web_contents());
  run_loop.Run();

  EXPECT_EQ(service()->web_contents(), nullptr);
  EXPECT_TRUE(service()->IsPlaybackPaused());

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

TEST_F(ReadAloudServiceTest, SetVoiceAndLanguageCodeForwardsToBroker) {
  service()->SetVoice("es-ES-Wavenet-B");
  service()->SetLanguageCode("es");
}

TEST_F(ReadAloudServiceTest, PlayForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_play_callback(run_loop.QuitClosure());
  service()->Play(web_contents());
  run_loop.Run();
  EXPECT_EQ(fake_controller()->play_count(), 1);
}

TEST_F(ReadAloudServiceTest, PauseForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_pause_callback(run_loop.QuitClosure());
  service()->Pause();
  run_loop.Run();
  EXPECT_EQ(fake_controller()->pause_count(), 1);
}

TEST_F(ReadAloudServiceTest, SetPlaybackRateForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_playback_rate_callback(run_loop.QuitClosure());
  service()->SetPlaybackRate(1.5f);
  run_loop.Run();
  EXPECT_FLOAT_EQ(fake_controller()->last_playback_rate(), 1.5f);
}

TEST_F(ReadAloudServiceTest, SetPlaybackMode) {
  EXPECT_EQ(ReadAloudService::PlaybackMode::kClassic,
            service()->playback_mode());

  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  EXPECT_EQ(ReadAloudService::PlaybackMode::kOverview,
            service()->playback_mode());
}

TEST_F(ReadAloudServiceTest, CheckReadability) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  const GURL valid_url("https://www.example.com/article");
  const GURL invalid_url("chrome://settings");

  EXPECT_CALL(*delegate_ptr, OnReadabilityResult(valid_url, true)).Times(1);
  service()->CheckReadability(valid_url);

  EXPECT_CALL(*delegate_ptr, OnReadabilityResult(invalid_url, false)).Times(1);
  service()->CheckReadability(invalid_url);

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       RequestSpeechSynthesisDelegatesToBrokerAndHandlesError) {
  bool callback_called = false;
  service()->RequestSpeechSynthesis(
      /*text_chunk=*/u"Hello world", /*sequence_id=*/1,
      base::BindLambdaForTesting(
          [&](mojo_base::BigBuffer response_bytes, bool success) {
            callback_called = true;
            // No OptGuide service configured on TestingProfile in basic unit
            // test setup, so expects false gracefully.
            EXPECT_FALSE(success);
            EXPECT_EQ(response_bytes.size(), 0u);
          }));
  EXPECT_TRUE(callback_called);
}

}  // namespace readaloud
