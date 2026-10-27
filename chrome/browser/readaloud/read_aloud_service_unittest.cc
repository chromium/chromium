// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"
#include "chrome/browser/readaloud/fake_audio_stream_factory.h"
#include "chrome/browser/readaloud/read_aloud_service_factory.h"
#include "chrome/browser/readaloud/read_aloud_voice_catalog.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_player_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

namespace {

constexpr char kTestVoiceId[] = "msf00006";
constexpr std::u16string_view kTestPreviewText = u"Voice preview sample text.";

class TestDistillerPage : public dom_distiller::test::MockDistillerPage {
 public:
  TestDistillerPage() = default;
  ~TestDistillerPage() override = default;

  int GetMinContentLength() const {
    return GetMinimumAllowableDistilledContentLength();
  }
};

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
              (read_aloud::mojom::PlaybackState playback_state),
              (override));
  MOCK_METHOD(void,
              OnVoicesAvailable,
              (base::span<const VoiceInfo> voices),
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
               read_aloud::mojom::PlaybackState playback_state),
              (override));
  MOCK_METHOD(void,
              OnReadabilityResult,
              (const GURL& url, bool is_readable),
              (override));
  MOCK_METHOD(void, OnNativeDestroyed, (), (override));
  MOCK_METHOD(void,
              OnTextChunked,
              (const std::vector<std::u16string>&),
              (override));
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
          receiver,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client) {
    receivers_.Add(this, std::move(receiver));
    if (client_.is_bound()) {
      client_.set_disconnect_handler(base::NullCallback());
      previous_clients_.push_back(std::move(client_));
    }
    client_.Bind(std::move(client));
    client_.set_disconnect_handler(base::BindOnce(
        &FakePlaybackController::OnClientDisconnected, base::Unretained(this)));
  }

  void Reset() {
    receivers_.Clear();
    client_.reset();
    previous_clients_.clear();
    received_segments_.clear();
    last_audio_stream_.reset();
    last_data_pipe_.reset();
    play_count_ = 0;
    pause_count_ = 0;
    set_overview_content_called_count_ = 0;
    play_count_at_set_overview_content_ = 0;
    set_overview_content_override_ = false;
  }

  void FlushForTesting() {
    receivers_.FlushForTesting();
    for (mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient>&
             prev_client : previous_clients_) {
      if (prev_client.is_bound()) {
        prev_client.FlushForTesting();
      }
    }
    if (client_.is_bound()) {
      client_.FlushForTesting();
    }
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

  void SetPlaybackMode(read_aloud::mojom::PlaybackMode mode) override {
    playback_mode_ = mode;
  }

  void SetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments) override {
    received_segments_ = std::move(segments);
    if (set_text_content_callback_) {
      std::move(set_text_content_callback_).Run();
    }
  }
  void SetOverviewContent(mojo_base::BigBuffer response_bytes,
                          SetOverviewContentCallback callback) override {
    set_overview_content_called_count_++;
    play_count_at_set_overview_content_ = play_count_;
    if (set_overview_content_override_) {
      std::move(callback).Run(set_overview_content_success_, /*title=*/"");
      return;
    }
    optimization_guide::proto::ReadAloudGenerateTextResponse response;
    if (response_bytes.size() > 0 &&
        response.ParseFromArray(response_bytes.data(),
                                response_bytes.size()) &&
        !response.dialogue_turns().empty()) {
      auto segment = read_aloud::mojom::TextSegment::New();
      segment->text =
          base::UTF8ToUTF16(response.dialogue_turns(0).utterance());
      received_segments_.push_back(std::move(segment));
      std::move(callback).Run(/*success=*/true, response.title());
      return;
    }
    std::move(callback).Run(/*success=*/false, /*title=*/"");
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
  void SeekToWord(uint32_t segment_index, uint32_t character_offset) override {
    last_seek_segment_index_ = segment_index;
    last_seek_character_offset_ = character_offset;
    if (seek_to_word_callback_) {
      std::move(seek_to_word_callback_).Run();
    }
  }
  void SeekToTime(base::TimeDelta position) override {
    last_seek_time_ = position;
    if (seek_to_time_callback_) {
      std::move(seek_to_time_callback_).Run();
    }
  }
  void SeekRelative(base::TimeDelta offset) override {
    last_seek_relative_offset_ = offset;
    if (seek_relative_callback_) {
      std::move(seek_relative_callback_).Run();
    }
  }
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
  void set_seek_to_word_callback(base::OnceClosure callback) {
    seek_to_word_callback_ = std::move(callback);
  }
  void set_seek_to_time_callback(base::OnceClosure callback) {
    seek_to_time_callback_ = std::move(callback);
  }
  void set_seek_relative_callback(base::OnceClosure callback) {
    seek_relative_callback_ = std::move(callback);
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
  std::optional<uint32_t> last_seek_segment_index() const {
    return last_seek_segment_index_;
  }
  std::optional<uint32_t> last_seek_character_offset() const {
    return last_seek_character_offset_;
  }
  std::optional<base::TimeDelta> last_seek_time() const {
    return last_seek_time_;
  }
  std::optional<base::TimeDelta> last_seek_relative_offset() const {
    return last_seek_relative_offset_;
  }
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
  read_aloud::mojom::ReadAloudPlaybackControllerClient* client() {
    return client_.get();
  }

  read_aloud::mojom::PlaybackMode playback_mode() const {
    return playback_mode_;
  }

  int set_overview_content_called_count() const {
    return set_overview_content_called_count_;
  }
  int play_count_at_set_overview_content() const {
    return play_count_at_set_overview_content_;
  }
  void set_overview_content_success(bool success) {
    set_overview_content_override_ = true;
    set_overview_content_success_ = success;
  }

 private:
  void OnClientDisconnected() {
    client_.reset();
    while (!previous_clients_.empty()) {
      mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          prev_client = std::move(previous_clients_.back());
      previous_clients_.pop_back();
      if (prev_client.is_bound() && prev_client.is_connected()) {
        client_ = std::move(prev_client);
        client_.set_disconnect_handler(
            base::BindOnce(&FakePlaybackController::OnClientDisconnected,
                           base::Unretained(this)));
        break;
      }
    }
  }

  mojo::ReceiverSet<read_aloud::mojom::ReadAloudPlaybackController> receivers_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient> client_;
  std::vector<
      mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient>>
      previous_clients_;
  std::vector<read_aloud::mojom::TextSegmentPtr> received_segments_;
  mojo::PendingRemote<media::mojom::AudioOutputStream> last_audio_stream_;
  media::mojom::ReadWriteAudioDataPipePtr last_data_pipe_;
  media::AudioParameters last_audio_params_;
  base::OnceClosure set_text_content_callback_;
  base::OnceClosure play_callback_;
  base::OnceClosure pause_callback_;
  base::OnceClosure seek_to_word_callback_;
  base::OnceClosure seek_to_time_callback_;
  base::OnceClosure seek_relative_callback_;
  base::OnceClosure set_playback_rate_callback_;
  int play_count_ = 0;
  int pause_count_ = 0;
  std::optional<uint32_t> last_seek_segment_index_;
  std::optional<uint32_t> last_seek_character_offset_;
  std::optional<base::TimeDelta> last_seek_time_;
  std::optional<base::TimeDelta> last_seek_relative_offset_;
  float last_playback_rate_ = 1.0f;
  base::OnceClosure initialize_audio_callback_;
  int initialize_audio_called_count_ = 0;
  read_aloud::mojom::PlaybackMode playback_mode_ =
      read_aloud::mojom::PlaybackMode::kClassic;
  int set_overview_content_called_count_ = 0;
  int play_count_at_set_overview_content_ = 0;
  bool set_overview_content_override_ = false;
  bool set_overview_content_success_ = true;
};

using SuspendType = media_session::mojom::MediaSession::SuspendType;

// A normal MediaSession player that stands in for page audio playing in the
// same tab as Read Aloud. Unlike a web player, it never reports state changes
// back to the MediaSession.
class FakePagePlayer : public content::MediaSessionPlayerObserver {
 public:
  explicit FakePagePlayer(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  FakePagePlayer(const FakePagePlayer&) = delete;
  FakePagePlayer& operator=(const FakePagePlayer&) = delete;
  ~FakePagePlayer() override {
    if (content::MediaSession* session =
            content::MediaSession::GetIfExists(web_contents_)) {
      session->RemovePlayer(this, kPlayerId);
    }
  }

  // Adds this player to the tab's MediaSession as playing. Returns whether
  // the MediaSession accepted it.
  bool StartPlaying() {
    is_paused_ = false;
    return content::MediaSession::Get(web_contents_)
        ->AddPlayer(this, kPlayerId);
  }

  int suspend_count() const { return suspend_count_; }
  int resume_count() const { return resume_count_; }

  // content::MediaSessionPlayerObserver:
  void OnSuspend(int player_id, bool triggered_by_user) override {
    ++suspend_count_;
    is_paused_ = true;
  }
  void OnResume(int player_id, bool triggered_by_user) override {
    ++resume_count_;
    is_paused_ = false;
  }
  bool HasAudio(int player_id) const override { return true; }
  bool IsPaused(int player_id) const override { return is_paused_; }
  content::RenderFrameHost* render_frame_host() const override {
    return web_contents_->GetPrimaryMainFrame();
  }

 private:
  static constexpr int kPlayerId = 0;

  const raw_ptr<content::WebContents> web_contents_;
  bool is_paused_ = true;
  int suspend_count_ = 0;
  int resume_count_ = 0;
};

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

  void TearDown() override {
    if (service() && service()->delegate()) {
      EXPECT_CALL(
          *static_cast<MockDelegate*>(service()->delegate()),
          OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped))
          .Times(testing::AnyNumber());
    }
    ChromeRenderViewHostTestHarness::TearDown();
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

  // If `out_delegate` is non-null, captures the ViewRequestDelegate so the
  // test can deliver the distilled article.
  void ExpectDistillation(
      const GURL& url,
      dom_distiller::ViewRequestDelegate** out_delegate = nullptr) {
    EXPECT_CALL(*mock_distiller_service(),
                CreateDefaultDistillerPageWithHandle(testing::_))
        .WillOnce(testing::Return(testing::ByMove(
            std::make_unique<dom_distiller::test::MockDistillerPage>())));

    EXPECT_CALL(*mock_distiller_service(),
                ViewUrlIgnoreCache(service(), testing::_, url))
        .WillOnce(
            [out_delegate](dom_distiller::ViewRequestDelegate* delegate,
                           std::unique_ptr<dom_distiller::DistillerPage> page,
                           const GURL& url) {
              if (out_delegate) {
                *out_delegate = delegate;
              }
              return std::make_unique<dom_distiller::ViewerHandle>(
                  base::DoNothing());
            });
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
                OnPlaybackStateChanged(
                    read_aloud::mojom::PlaybackState::kPlaybackCreation))
        .Times(1);
    EXPECT_CALL(*delegate,
                OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(0),
                                          /*duration=*/base::Seconds(0)))
        .Times(1);
  }

  void BindController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client) {
    if (fake_controller_) {
      fake_controller_->Bind(std::move(receiver), std::move(client));
    }
  }

  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    fake_audio_stream_factory_.Bind(std::move(receiver));
  }

  MockDelegate* SetUpMockDelegate() {
    auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
    MockDelegate* delegate_ptr = delegate.get();
    service()->SetDelegate(std::move(delegate));
    return delegate_ptr;
  }

  MockOptimizationGuideKeyedService* SetUpMockOptimizationGuide() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating([](content::BrowserContext*)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      testing::NiceMock<MockOptimizationGuideKeyedService>>();
                })));
  }

  // Returns a MES response with a one-turn script.
  static optimization_guide::proto::ReadAloudGenerateTextResponse
  OneTurnOverviewResponse() {
    optimization_guide::proto::ReadAloudGenerateTextResponse response;
    response.add_dialogue_turns()->set_utterance("Overview script.");
    return response;
  }

  // Starts a kOverview session whose mocked MES call returns `response`.
  // Returns the distiller delegate used to deliver the article.
  dom_distiller::ViewRequestDelegate* StartOverviewSession(
      const optimization_guide::proto::ReadAloudGenerateTextResponse& response =
          OneTurnOverviewResponse()) {
    const GURL url("https://www.example.com/article");
    NavigateAndCommit(url);
    SetFakeController(std::make_unique<FakePlaybackController>());

    MockOptimizationGuideKeyedService* opt_guide =
        SetUpMockOptimizationGuide();
    EXPECT_CALL(
        *opt_guide,
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
            testing::_, testing::_, testing::_))
        .Times(testing::AtMost(1))
        .WillOnce(
            [any_response = optimization_guide::AnyWrapProto(response)](
                optimization_guide::ModelBasedCapabilityKey feature,
                const google::protobuf::MessageLite& request_metadata,
                const optimization_guide::ModelExecutionOptions& options,
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      any_response, /*execution_info=*/nullptr),
                  /*log_entry=*/nullptr);
            });

    dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
    ExpectDistillation(url, &view_delegate);
    // Matches production, where the mode is set before Initialize().
    service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
    service()->Initialize(web_contents());
    return view_delegate;
  }

  // Delivers a single-page distilled article, which in kOverview mode
  // triggers overview generation.
  void DeliverDistilledArticle(dom_distiller::ViewRequestDelegate* delegate) {
    const int previous_count =
        fake_controller()->set_overview_content_called_count();
    dom_distiller::DistilledArticleProto proto;
    proto.add_pages()->set_text_content("Distilled article content");
    delegate->OnArticleReady(&proto);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return fake_controller()->set_overview_content_called_count() >
             previous_count;
    }));
    // Flush the FakePlaybackController receiver so the SetOverviewContent
    // reply callback (ReadAloudService::OnOverviewContentSet) completes.
    fake_controller()->FlushForTesting();
  }

 private:
  std::unique_ptr<FakePlaybackController> fake_controller_;
  FakeAudioStreamFactory fake_audio_stream_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAloudServiceTest, DistillNullWebContents) {
  // Should be a completely safe no-op.
  service()->Initialize(nullptr);
  EXPECT_EQ(GetViewerHandle(), nullptr);
}

TEST_F(ReadAloudServiceTest,
       DistillPageSetsMinimumAllowableContentLengthToZero) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  int captured_min_length = -1;

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(
          testing::ByMove(std::make_unique<TestDistillerPage>())));

  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* delegate,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        if (page) {
          captured_min_length = static_cast<TestDistillerPage*>(page.get())
                                    ->GetMinContentLength();
        }
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  service()->Initialize(web_contents());

  EXPECT_EQ(captured_min_length, 0);
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
  page1->set_text_content("First page content");
  dom_distiller::DistilledPageProto* page2 = proto.add_pages();
  page2->set_text_content("Second page content");

  delegate_ptr->OnArticleReady(&proto);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetViewerHandle(), nullptr);
  histograms.ExpectTotalCount("ReadAloud.Distillation.Duration", 1);
  histograms.ExpectUniqueSample("ReadAloud.Distillation.Success", true, 1);

  const std::vector<read_aloud::mojom::TextSegmentPtr>& segments =
      fake_controller()->received_segments();
  ASSERT_EQ(segments.size(), 2u);
  EXPECT_EQ(segments[0]->segment_index, 0u);
  EXPECT_EQ(segments[0]->text, u"First page content");
  EXPECT_EQ(segments[1]->segment_index, 1u);
  EXPECT_EQ(segments[1]->text, u"Second page content");
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReadyWithEmptyPage) {
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
  page1->set_text_content("Page 1 text");
  dom_distiller::DistilledPageProto* page2 = proto.add_pages();
  page2->set_text_content("");

  delegate_ptr->OnArticleReady(&proto);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetViewerHandle(), nullptr);

  const std::vector<read_aloud::mojom::TextSegmentPtr>& segments =
      fake_controller()->received_segments();
  ASSERT_EQ(segments.size(), 2u);
  EXPECT_EQ(segments[0]->segment_index, 0u);
  EXPECT_EQ(segments[0]->text, u"Page 1 text");
  EXPECT_EQ(segments[1]->segment_index, 1u);
  EXPECT_EQ(segments[1]->text, u"");
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
              OnPlaybackStateChanged(
                  read_aloud::mojom::PlaybackState::kPlaybackCreation))
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
              OnPlaybackStateChanged(
                  read_aloud::mojom::PlaybackState::kPlaybackCreation))
      .Times(1);
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(0),
                                        /*duration=*/base::Seconds(0)))
      .Times(1);

  service()->Initialize(test_contents.get());

  // Destruction of test_contents triggers WebContentsDestroyed(), which stops
  // playback.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
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

TEST_F(ReadAloudServiceTest, OnPlaybackProgressChangedForwardsToDelegate) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  SetFakeController(std::make_unique<FakePlaybackController>());

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackProgressUpdated(/*elapsed=*/base::Seconds(10),
                                        /*duration=*/base::Seconds(120)))
      .Times(1);
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(10), /*duration=*/base::Seconds(120));
  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, OnPlaybackProgressChangedRejectsNegativeElapsed) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(-1), /*duration=*/base::Seconds(120));

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: invalid playback progress received from utility");
}

TEST_F(ReadAloudServiceTest, OnPlaybackProgressChangedRejectsNegativeDuration) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(0), /*duration=*/base::Seconds(-1));

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: invalid playback progress received from utility");
}

TEST_F(ReadAloudServiceTest,
       OnPlaybackProgressChangedRejectsDurationExceedingMax) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(0),
      /*duration=*/ReadAloudService::kMaxPlaybackDuration + base::Seconds(1));

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: invalid playback progress received from utility");
}

TEST_F(ReadAloudServiceTest, OnPlaybackProgressChangedRejectsMaxDuration) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(0), /*duration=*/base::TimeDelta::Max());

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: invalid playback progress received from utility");
}

TEST_F(ReadAloudServiceTest,
       OnPlaybackProgressChangedRejectsElapsedPastDuration) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackProgressChanged(
      /*elapsed=*/base::Seconds(121), /*duration=*/base::Seconds(120));

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: invalid playback progress received from utility");
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
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(testing::AtMost(1));
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
  EXPECT_CALL(
      *delegate_ptr_mock,
      OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
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

  ExpectInitializeCallbacks(delegate_ptr_mock);
  service()->Initialize(web_contents());

  EXPECT_NE(GetViewerHandle(), nullptr);
  ASSERT_NE(delegate_ptr, nullptr);

  EXPECT_CALL(
      *delegate_ptr_mock,
      OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  EXPECT_CALL(*delegate_ptr_mock, OnPlaybackError("Distillation failed")).Times(1);
  EXPECT_CALL(*delegate_ptr_mock, OnNativeDestroyed()).Times(1);

  // Simulate DomDistiller finishing distillation with failure (no pages).
  dom_distiller::DistilledArticleProto proto;
  delegate_ptr->OnDistillationFailed(
      dom_distiller::DistillationParseResult::kContentTooShort);
  delegate_ptr->OnArticleReady(&proto);

  EXPECT_EQ(GetViewerHandle(), nullptr);
  histograms.ExpectTotalCount("ReadAloud.Distillation.Duration", 1);
  histograms.ExpectUniqueSample("ReadAloud.Distillation.Success", false, 1);
  histograms.ExpectUniqueSample(
      "ReadAloud.Distillation.FailureReason",
      dom_distiller::DistillationParseResult::kContentTooShort, 1);
}

TEST_F(ReadAloudServiceTest, OnArticleUpdated) {
  dom_distiller::ArticleDistillationUpdate update({}, false, false);
  // Should be a completely safe no-op.
  auto* delegate = static_cast<dom_distiller::ViewRequestDelegate*>(service());
  delegate->OnArticleUpdated(update);
}

TEST_F(ReadAloudServiceTest, ShutdownClearsHandle) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  ExpectDistillation(GURL("https://www.example.com/article"));

  service()->Play(web_contents());
  EXPECT_NE(GetViewerHandle(), nullptr);

  service()->Shutdown();
  EXPECT_EQ(GetViewerHandle(), nullptr);
  EXPECT_EQ(service()->web_contents(), nullptr);
}

TEST_F(ReadAloudServiceTest, StopDetachesWebContentsObserver) {
  service()->Play(web_contents());
  EXPECT_EQ(service()->web_contents(), web_contents());

  service()->Stop();
  EXPECT_EQ(service()->web_contents(), nullptr);
}

TEST_F(ReadAloudServiceTest, PlayNullWebContents) {
  // Should safely return early without crashing or modifying web_contents.
  service()->Play(nullptr);
  EXPECT_EQ(service()->web_contents(), nullptr);
}

TEST_F(ReadAloudServiceTest, SetDelegateAndShutdownLifecycle) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();

  // Initially, there is no delegate.
  EXPECT_EQ(service()->delegate(), nullptr);

  // Registering the delegate should succeed and be accessible.
  service()->SetDelegate(std::move(delegate));
  EXPECT_EQ(service()->delegate(), delegate_ptr);

  // Shutdown should trigger OnNativeDestroyed() exactly once and clear the
  // delegate.
  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
  service()->Shutdown();
  EXPECT_EQ(service()->delegate(), nullptr);
}

TEST_F(ReadAloudServiceTest, PrimaryPageChangedStopsAndDetachesObserver) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  ExpectDistillation(GURL("https://www.example.com/article"));

  service()->Play(web_contents());
  EXPECT_NE(GetViewerHandle(), nullptr);

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);

  // Navigating to a new URL triggers PrimaryPageChanged().
  NavigateAndCommit(GURL("https://www.example.com/other"));

  EXPECT_EQ(GetViewerHandle(), nullptr);
  EXPECT_EQ(service()->web_contents(), nullptr);

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
  EXPECT_EQ(service()->web_contents(), web_contents());
  ASSERT_NE(distiller_delegate, nullptr);

  // Simulate article ready - should connect to utility player and bind.
  dom_distiller::DistilledArticleProto proto;
  dom_distiller::DistilledPageProto* page = proto.add_pages();
  page->set_html("Content");
  base::RunLoop run_loop;
  fake_controller()->set_text_content_callback(run_loop.QuitClosure());
  distiller_delegate->OnArticleReady(&proto);
  run_loop.Run();

  // Verify segments were received by fake controller.
  EXPECT_EQ(fake_controller()->received_segments().size(), 1u);

  // Call Stop() - should disconnect utility player.
  service()->Stop();
  EXPECT_EQ(service()->web_contents(), nullptr);

  // Reset fake controller to ensure we can detect a new connection.
  fake_controller()->Reset();

  // Call Play() again (Second Session) - should start a second distillation
  // because we previously stopped the session.
  distiller_delegate = nullptr;
  service()->Play(web_contents());
  ASSERT_NE(distiller_delegate, nullptr);

  // Simulate article ready again - should reconnect.
  base::RunLoop run_loop2;
  fake_controller()->set_text_content_callback(run_loop2.QuitClosure());
  distiller_delegate->OnArticleReady(&proto);
  run_loop2.Run();

  // Verify new segments were received (proving reconnection).
  EXPECT_EQ(fake_controller()->received_segments().size(), 1u);

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
      .WillOnce([&run_loop] { run_loop.Quit(); });

  service()->Play(web_contents());
  run_loop.Run();

  EXPECT_EQ(service()->web_contents(), nullptr);

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest,
       WebContentsDestroyedStopsAndDetachesObserver) {
  std::unique_ptr<content::WebContents> test_contents =
      CreateTestWebContents();
  service()->Play(test_contents.get());
  EXPECT_EQ(service()->web_contents(), test_contents.get());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);

  // Deleting the observed WebContents triggers WebContentsDestroyed().
  test_contents.reset();

  EXPECT_EQ(GetViewerHandle(), nullptr);
  EXPECT_EQ(service()->web_contents(), nullptr);

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, VoicePreviewDispatchesPlayingAndStoppedStates) {
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  service()->PreviewVoice(kTestVoiceId, kTestPreviewText);

  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  kTestVoiceId, read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();

  service()->PauseVoicePreview();
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->pause_count(), 1);

  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  kTestVoiceId, read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();

  // Calling PreviewVoice with the same voice ID resumes the paused preview.
  service()->PreviewVoice(kTestVoiceId, kTestPreviewText);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->play_count(), 2);

  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  /*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->StopVoicePreview();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, PreviewVoicePausesActivePlayback) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Play(web_contents());
  EXPECT_EQ(service()->web_contents(), web_contents());

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  // PreviewVoice should pause active article playback (delegating the article
  // kPaused notification to the utility process) and start the requested voice
  // preview without disconnecting the paused article controller pipe.
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  service()->PreviewVoice(kTestVoiceId, kTestPreviewText);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->pause_count(), 1);

  // Stopping playback stops the active voice preview and returns article state
  // to stopped before teardown.
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  /*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, PlayResumesPlaybackAfterVoicePreview) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Play(web_contents());
  service()->PreviewVoice(kTestVoiceId, kTestPreviewText);
  fake_controller()->FlushForTesting();

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  // Starting article playback again stops the active voice preview and resumes
  // article playback (delegating article kPlaying notification to the utility
  // process).
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  /*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Play(web_contents());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->play_count(), 3);

  // Because Play() resets the preview controller's utility connection,
  // subsequent utility state updates are routed to the article playback client.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();

  // Explicitly stopping playback returns article state to stopped before
  // teardown.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, InitializeStopsActiveVoicePreview) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->PreviewVoice(kTestVoiceId, kTestPreviewText);

  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  testing::InSequence s;
  EXPECT_CALL(*delegate_ptr,
              OnVoicePreviewPlaybackStateChanged(
                  /*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  ExpectInitializeCallbacks(delegate_ptr, /*expected_title=*/"",
                            /*expected_publisher=*/"");
  service()->Initialize(web_contents());

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

class ReadAloudServiceMediaSessionTest : public ReadAloudServiceTest {
 public:
  void SetUp() override {
    ReadAloudServiceTest::SetUp();
    // Grants audio focus without calling into the Android AudioManager.
    audio_focus_feature_list_.InitAndDisableFeature(
        media::kRequestSystemAudioFocus);
  }

  content::MediaSession* media_session() {
    return content::MediaSession::Get(web_contents());
  }

  // Starts Read Aloud playback, which adds Read Aloud to the tab's
  // MediaSession as a playing player once the utility reports `kPlaying`.
  void StartReadAloud() {
    NavigateAndCommit(GURL("https://www.example.com/article"));
    SetFakeController(std::make_unique<FakePlaybackController>());
    service()->Initialize(web_contents());

    base::RunLoop run_loop;
    fake_controller()->set_play_callback(run_loop.QuitClosure());
    service()->Play(web_contents());
    run_loop.Run();
    fake_controller()->client()->OnPlaybackStateChanged(
        read_aloud::mojom::PlaybackState::kPlaying);
    fake_controller()->FlushForTesting();
  }

  void SuspendAndWaitForUtilityPause(SuspendType suspend_type) {
    base::RunLoop run_loop;
    fake_controller()->set_pause_callback(run_loop.QuitClosure());
    media_session()->Suspend(suspend_type);
    run_loop.Run();
  }

  void ResumeAndWaitForUtilityPlay(SuspendType suspend_type) {
    base::RunLoop run_loop;
    fake_controller()->set_play_callback(run_loop.QuitClosure());
    media_session()->Resume(suspend_type);
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList audio_focus_feature_list_;
};

class ReadAloudServiceMediaSessionSuspendTest
    : public ReadAloudServiceMediaSessionTest,
      public testing::WithParamInterface<SuspendType> {};

// Regression test for b/572721812: suspending the MediaSession while page
// audio and Read Aloud are both playing must not make Read Aloud remove itself
// from the MediaSession while the MediaSession iterates over its players.
// Without the fix this crashes in MediaSessionImpl::OnSuspendInternal().
TEST_P(ReadAloudServiceMediaSessionSuspendTest,
       SuspendWithPageAudioPausesReadAloud) {
  MockDelegate* delegate = SetUpMockDelegate();
  StartReadAloud();
  FakePagePlayer page_player(web_contents());
  ASSERT_TRUE(page_player.StartPlaying());

  EXPECT_CALL(*delegate,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  SuspendAndWaitForUtilityPause(GetParam());
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();

  EXPECT_EQ(fake_controller()->pause_count(), 1);
  EXPECT_EQ(page_player.suspend_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAloudServiceMediaSessionSuspendTest,
                         testing::Values(SuspendType::kUI,
                                         SuspendType::kSystem),
                         [](const testing::TestParamInfo<SuspendType>& info) {
                           return info.param == SuspendType::kUI ? "UI"
                                                                 : "System";
                         });

// After a suspend with page audio playing, Read Aloud stays in the
// MediaSession as a paused player, so resuming the session resumes it.
TEST_F(ReadAloudServiceMediaSessionTest,
       ResumeWithPageAudioResumesReadAloudAndPageAudio) {
  StartReadAloud();
  FakePagePlayer page_player(web_contents());
  ASSERT_TRUE(page_player.StartPlaying());
  SuspendAndWaitForUtilityPause(SuspendType::kUI);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();

  ResumeAndWaitForUtilityPlay(SuspendType::kUI);

  // One Play() from StartReadAloud() and one from the resume.
  EXPECT_EQ(fake_controller()->play_count(), 2);
  EXPECT_EQ(page_player.resume_count(), 1);
}

// A pause reported by the utility that was not initiated by MediaSession must
// notify MediaSession::OnPlayerPaused(). With page audio also playing, that
// removes Read Aloud from the session so a later suspend no longer reaches it.
TEST_F(ReadAloudServiceMediaSessionTest,
       UtilityPausedNotifiesMediaSessionAndRemovesFromMultiPlayerSession) {
  MockDelegate* delegate = SetUpMockDelegate();
  StartReadAloud();
  FakePagePlayer page_player(web_contents());
  ASSERT_TRUE(page_player.StartPlaying());

  EXPECT_CALL(*delegate,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();

  media_session()->Suspend(SuspendType::kUI);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(page_player.suspend_count(), 1);
  EXPECT_EQ(fake_controller()->pause_count(), 0);
}

// If a session-initiated suspend never receives a `kPaused` report from the
// utility, resuming and then pausing must still reach `MediaSession`.
TEST_F(ReadAloudServiceMediaSessionTest,
       UserPauseAfterSessionSuspendAndResumeReachesMediaSession) {
  MockDelegate* delegate = SetUpMockDelegate();
  StartReadAloud();
  FakePagePlayer page_player(web_contents());
  ASSERT_TRUE(page_player.StartPlaying());

  // Session suspends and resumes without the utility reporting `kPaused` in
  // between.
  SuspendAndWaitForUtilityPause(SuspendType::kUI);
  ResumeAndWaitForUtilityPlay(SuspendType::kUI);
  EXPECT_EQ(fake_controller()->pause_count(), 1);
  EXPECT_EQ(fake_controller()->play_count(), 2);

  // User pauses Read Aloud; the utility answers with `kPaused`.
  EXPECT_CALL(*delegate,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  service()->Pause();
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->pause_count(), 2);

  // Read Aloud was removed from the multi-player MediaSession on pause, so
  // suspending the session now only suspends `page_player`.
  media_session()->Suspend(SuspendType::kUI);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(page_player.suspend_count(), 2);
  EXPECT_EQ(fake_controller()->pause_count(), 2);
}

// Same as above when the user resumes via `ReadAloudService::Play()` rather
// than `MediaSession::Resume()` after an unanswered session suspend.
TEST_F(ReadAloudServiceMediaSessionTest,
       UserPlayAndPauseAfterUnansweredSessionSuspendReachesMediaSession) {
  StartReadAloud();
  FakePagePlayer page_player(web_contents());
  ASSERT_TRUE(page_player.StartPlaying());

  // Session suspends, setting `pending_session_command_ = kSuspend`, and the
  // utility never reports `kPaused`.
  SuspendAndWaitForUtilityPause(SuspendType::kUI);
  EXPECT_EQ(fake_controller()->pause_count(), 1);

  // User plays and then pauses from the Read Aloud UI.
  // `NotifyPlaybackStarted()` on `kPlaying` clears the stale `kSuspend` command
  // so `kPaused` reaches MediaSession.
  service()->Play(web_contents());
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();
  ASSERT_TRUE(page_player.StartPlaying());
  service()->Pause();
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->pause_count(), 2);

  media_session()->Suspend(SuspendType::kUI);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(page_player.suspend_count(), 2);
  EXPECT_EQ(fake_controller()->pause_count(), 2);
}

// `kEndOfStream` keeps Read Aloud in the MediaSession as a paused player (not
// `RemovePlayer`), so resuming the session routes `Play()` back to Read Aloud.
TEST_F(ReadAloudServiceMediaSessionTest,
       EndOfStreamKeepsPlayerInMediaSessionAndPlayRestarts) {
  MockDelegate* delegate = SetUpMockDelegate();
  StartReadAloud();

  EXPECT_CALL(*delegate, OnPlaybackStateChanged(
                             read_aloud::mojom::PlaybackState::kEndOfStream))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kEndOfStream);
  fake_controller()->FlushForTesting();

  // Because Read Aloud stays in MediaSession as a paused player at
  // kEndOfStream, resuming the MediaSession reaches Read Aloud and calls
  // Play().
  ResumeAndWaitForUtilityPlay(SuspendType::kUI);
  EXPECT_EQ(fake_controller()->play_count(), 2);

  EXPECT_CALL(*delegate, OnPlaybackStateChanged(
                             read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();
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

TEST_F(ReadAloudServiceTest, SeekForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_seek_to_time_callback(run_loop.QuitClosure());
  service()->Seek(base::Seconds(12));
  run_loop.Run();
  EXPECT_EQ(fake_controller()->last_seek_time(), base::Seconds(12));
}

TEST_F(ReadAloudServiceTest, SeekIgnoresNegativeAndMaxTime) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  service()->Seek(base::Seconds(-1));
  service()->Seek(base::TimeDelta::Max());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->last_seek_time(), std::nullopt);
}

TEST_F(ReadAloudServiceTest, SeekRelativeForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_seek_relative_callback(run_loop.QuitClosure());
  service()->SeekRelative(base::Seconds(10));
  run_loop.Run();
  EXPECT_EQ(fake_controller()->last_seek_relative_offset(), base::Seconds(10));
}

TEST_F(ReadAloudServiceTest, SeekRelativeNegativeOffsetForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_seek_relative_callback(run_loop.QuitClosure());
  service()->SeekRelative(base::Seconds(-5));
  run_loop.Run();
  EXPECT_EQ(fake_controller()->last_seek_relative_offset(), base::Seconds(-5));
}

TEST_F(ReadAloudServiceTest, SeekRelativeIgnoresMinAndMaxOffset) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  service()->SeekRelative(base::TimeDelta::Max());
  service()->SeekRelative(base::TimeDelta::Min());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->last_seek_relative_offset(), std::nullopt);
}

TEST_F(ReadAloudServiceTest, SeekToWordForwardedToUtility) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  base::RunLoop run_loop;
  fake_controller()->set_seek_to_word_callback(run_loop.QuitClosure());
  service()->SeekToWord(/*segment_index=*/2, /*character_offset=*/15);
  run_loop.Run();
  EXPECT_EQ(fake_controller()->last_seek_segment_index(), 2u);
  EXPECT_EQ(fake_controller()->last_seek_character_offset(), 15u);
}

TEST_F(ReadAloudServiceTest, SeekToWordIgnoresNegativeIndices) {
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  service()->SeekToWord(/*segment_index=*/-1, /*character_offset=*/0);
  service()->SeekToWord(/*segment_index=*/0, /*character_offset=*/-1);
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->last_seek_segment_index(), std::nullopt);
  EXPECT_EQ(fake_controller()->last_seek_character_offset(), std::nullopt);
}

TEST_F(ReadAloudServiceTest, SetPlaybackMode) {
  EXPECT_EQ(service()->playback_mode(),
            ReadAloudService::PlaybackMode::kClassic);

  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  EXPECT_EQ(service()->playback_mode(),
            ReadAloudService::PlaybackMode::kOverview);
}

TEST_F(ReadAloudServiceTest,
       OverviewModeDefersPlayUntilScriptLoadsAndResumesAfterPause) {
  base::HistogramTester histograms;
  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);

  // Initial Play() call defers utility's Play() until overview script loads.
  service()->Play(web_contents());
  EXPECT_EQ(fake_controller()->play_count(), 0);

  // Delivering the distilled article triggers MES generation, which then
  // parses the response and calls utility's Play().
  DeliverDistilledArticle(view_delegate);
  EXPECT_EQ(fake_controller()->play_count(), 1);

  histograms.ExpectUniqueSample("ReadAloud.Overview.Outcome",
                                ReadAloudService::OverviewOutcome::kSuccess, 1);
  histograms.ExpectTotalCount("ReadAloud.Overview.TotalDuration", 1);

  // Once overview_script_loaded_ is true, pausing and calling Play() again
  // must immediately forward Play() to the utility process.
  service()->Pause();
  service()->Play(web_contents());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->play_count(), 2);
}

TEST_F(ReadAloudServiceTest, OverviewModeReinitializeResetsScriptLoadedState) {
  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);
  service()->Play(web_contents());
  DeliverDistilledArticle(view_delegate);
  EXPECT_EQ(fake_controller()->play_count(), 1);

  // Re-initializing the session (as a mode switch does) resets
  // overview_script_loaded_.
  ExpectDistillation(GURL("https://www.example.com/article"));
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  service()->Initialize(web_contents());

  // Play() in overview mode without a re-loaded script must defer Play() to
  // utility.
  service()->Play(web_contents());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->play_count(), 1);
}

TEST_F(ReadAloudServiceTest,
       OverviewModePlayWithoutPriorInitializeStartsPlaybackOnScriptLoad) {
  const GURL url("https://www.example.com/article");
  NavigateAndCommit(url);
  SetFakeController(std::make_unique<FakePlaybackController>());

  MockOptimizationGuideKeyedService* opt_guide = SetUpMockOptimizationGuide();
  EXPECT_CALL(
      *opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::AnyWrapProto(OneTurnOverviewResponse()),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  ExpectDistillation(url, &view_delegate);
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);

  // Calling Play() directly (when web_contents() is null) triggers Initialize()
  // internally and must preserve the user's intent to play once the script
  // loads.
  service()->Play(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  DeliverDistilledArticle(view_delegate);
  EXPECT_EQ(fake_controller()->play_count(), 1);
}

TEST_F(ReadAloudServiceTest,
       OverviewModeOnArticleReadyPopulatesRequestAndDisablesHighlighting) {
  const GURL url("https://www.example.com/article?query=1#ref");
  NavigateAndCommit(url);
  SetFakeController(std::make_unique<FakePlaybackController>());
  MockDelegate* delegate_ptr = SetUpMockDelegate();
  MockOptimizationGuideKeyedService* mock_opt_guide =
      SetUpMockOptimizationGuide();

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  ExpectDistillation(url, &view_delegate);
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  service()->Initialize(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  EXPECT_CALL(*delegate_ptr, OnHighlightingSupported(/*supported=*/false))
      .Times(1);
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            const auto& req = static_cast<
                const optimization_guide::proto::ReadAloudGenerateTextRequest&>(
                request_metadata);
            EXPECT_EQ(req.page_title(), "Distilled Title");
            EXPECT_EQ(req.page_content(), "Page 1 text\n\nPage 2 text");
            EXPECT_EQ(req.page_url(), "https://www.example.com/article");
            EXPECT_EQ(req.language_code(), kAiOverviewLanguageCode);
          });

  dom_distiller::DistilledArticleProto proto;
  proto.set_title("Distilled Title");
  proto.add_pages()->set_text_content("Page 1 text");
  proto.add_pages()->set_text_content("Page 2 text");
  view_delegate->OnArticleReady(&proto);

  EXPECT_THAT(fake_controller()->received_segments(), testing::IsEmpty());
}

TEST_F(ReadAloudServiceTest, OverviewModePauseDuringGenerationDoesNotAutoPlay) {
  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);

  service()->Play(web_contents());
  EXPECT_EQ(fake_controller()->play_count(), 0);

  // User pauses while overview generation is in-flight.
  service()->Pause();

  DeliverDistilledArticle(view_delegate);
  // utility Play() must not be called because user paused.
  EXPECT_EQ(fake_controller()->play_count(), 0);

  // Resuming playback after generation completes forwards Play() to utility.
  service()->Play(web_contents());
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->play_count(), 1);
}

TEST_F(ReadAloudServiceTest, OverviewModeGeneratedTitleUpdatesMetadata) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  optimization_guide::proto::ReadAloudGenerateTextResponse response =
      OneTurnOverviewResponse();
  response.set_title("Generated Overview Title");

  dom_distiller::ViewRequestDelegate* view_delegate =
      StartOverviewSession(response);
  ASSERT_NE(view_delegate, nullptr);

  // Overview generation delivers "Generated Overview Title".
  EXPECT_CALL(*delegate_ptr,
              OnMetadataAvailable("Generated Overview Title", testing::_))
      .Times(1);
  DeliverDistilledArticle(view_delegate);
}

TEST_F(ReadAloudServiceTest,
       OverviewModeReinitializeToClassicRestoresTabTitle) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  optimization_guide::proto::ReadAloudGenerateTextResponse response =
      OneTurnOverviewResponse();
  response.set_title("Generated Overview Title");

  dom_distiller::ViewRequestDelegate* view_delegate =
      StartOverviewSession(response);
  ASSERT_NE(view_delegate, nullptr);
  web_contents()->UpdateTitleForEntry(
      web_contents()->GetController().GetLastCommittedEntry(),
      u"Original Tab Title");
  DeliverDistilledArticle(view_delegate);

  // Switching to kClassic re-initializes the session, which restores the tab
  // title.
  EXPECT_CALL(*delegate_ptr,
              OnMetadataAvailable("Original Tab Title", "example.com"))
      .Times(1);
  ExpectDistillation(GURL("https://www.example.com/article"));
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kClassic);
  service()->Initialize(web_contents());
}

TEST_F(ReadAloudServiceTest, SetPlaybackModeDoesNotNotifyDelegate) {
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  MockDelegate* delegate_ptr = SetUpMockDelegate();
  SetFakeController(std::make_unique<FakePlaybackController>());

  service()->Initialize(test_contents.get());

  // SetPlaybackMode is a plain setter; metadata is only pushed by Initialize().
  EXPECT_CALL(*delegate_ptr, OnMetadataAvailable(/*title=*/testing::_,
                                                 /*publisher=*/testing::_))
      .Times(0);
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kClassic);
}

TEST_F(ReadAloudServiceTest,
       OverviewModeEmptyGeneratedTitleFallsBackToArticleTitle) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  // The default overview response has no title.
  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);

  base::RunLoop run_loop;
  {
    testing::InSequence sequence;
    // Distillation refines the title while the overview is generating.
    EXPECT_CALL(*delegate_ptr,
                OnMetadataAvailable("Distilled Headline Title", testing::_));
    // The overview script loads without a generated title, so the article
    // title is used as the fallback.
    EXPECT_CALL(*delegate_ptr,
                OnMetadataAvailable("Distilled Headline Title", testing::_))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  }

  dom_distiller::DistilledArticleProto proto;
  proto.set_title("Distilled Headline Title");
  proto.add_pages()->set_text_content("Distilled article content");
  view_delegate->OnArticleReady(&proto);
  run_loop.Run();
}

TEST_F(ReadAloudServiceTest, OverviewModeOversizedTitleIsTruncated) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  // Construct a title string that exceeds kMaxOverviewMetadataLength.
  std::string long_title(/*count=*/kMaxOverviewMetadataLength + 176,
                         /*ch=*/'a');

  optimization_guide::proto::ReadAloudGenerateTextResponse response =
      OneTurnOverviewResponse();
  response.set_title(long_title);

  dom_distiller::ViewRequestDelegate* view_delegate =
      StartOverviewSession(response);
  ASSERT_NE(view_delegate, nullptr);

  EXPECT_CALL(*delegate_ptr,
              OnMetadataAvailable(
                  std::string(/*count=*/kMaxOverviewMetadataLength, /*ch=*/'a'),
                  testing::_))
      .Times(1);

  DeliverDistilledArticle(view_delegate);
}

TEST_F(ReadAloudServiceTest,
       OverviewModeTitleTruncatesAtMultiByteUtf8BoundaryAndNeutralizesBidi) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  // Place a 3-byte UTF-8 character ("€" = 0xE2 0x82 0xAC) across the
  // kMaxOverviewMetadataLength boundary so byte-slicing would split the
  // codepoint, preceded by a BIDI override character (U+202E, "\xE2\x80\xAE").
  std::string prefix = "\xE2\x80\xAE";
  prefix.append(/*count=*/kMaxOverviewMetadataLength - prefix.size() - 1,
                /*ch=*/'a');
  std::string untrusted_title = prefix + "€";

  optimization_guide::proto::ReadAloudGenerateTextResponse response =
      OneTurnOverviewResponse();
  response.set_title(untrusted_title);

  dom_distiller::ViewRequestDelegate* view_delegate =
      StartOverviewSession(response);
  ASSERT_NE(view_delegate, nullptr);

  EXPECT_CALL(*delegate_ptr, OnMetadataAvailable(testing::_, testing::_))
      .WillOnce([](std::string_view sanitized_title, std::string_view) {
        EXPECT_LE(sanitized_title.size(), kMaxOverviewMetadataLength);
        EXPECT_TRUE(base::IsStringUTF8(sanitized_title));
        // SanitizeUserSuppliedString wraps or strips unclosed BIDI overrides so
        // the trailing multi-byte character across the boundary is dropped
        // cleanly without splitting UTF-8 codepoints.
        EXPECT_EQ(sanitized_title.find("€"), std::string_view::npos);
      });

  DeliverDistilledArticle(view_delegate);
}

TEST_F(ReadAloudServiceTest, OverviewModeUtilityParsingFailureTriggersError) {
  base::HistogramTester histograms;
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);

  fake_controller()->set_overview_content_success(/*success=*/false);

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackError("Overview content parsing failed"))
      .Times(1);

  DeliverDistilledArticle(view_delegate);

  histograms.ExpectUniqueSample(
      "ReadAloud.Overview.Outcome",
      ReadAloudService::OverviewOutcome::kContentParsingFailed, 1);
  histograms.ExpectTotalCount("ReadAloud.Overview.TotalDuration", 0);
}

TEST_F(ReadAloudServiceTest, OverviewModeGenerationFailureTriggersError) {
  base::HistogramTester histograms;
  MockDelegate* delegate_ptr = SetUpMockDelegate();
  MockOptimizationGuideKeyedService* mock_opt_guide =
      SetUpMockOptimizationGuide();

  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::proto::Any(),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  const GURL url("https://www.example.com/article");
  NavigateAndCommit(url);
  SetFakeController(std::make_unique<FakePlaybackController>());

  dom_distiller::ViewRequestDelegate* view_delegate = nullptr;
  ExpectDistillation(url, &view_delegate);
  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);
  service()->Initialize(web_contents());
  ASSERT_NE(view_delegate, nullptr);

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackError("Overview generation failed"))
      .Times(1);

  dom_distiller::DistilledArticleProto proto;
  proto.add_pages()->set_text_content("Distilled article content");
  view_delegate->OnArticleReady(&proto);

  histograms.ExpectUniqueSample(
      "ReadAloud.Overview.Outcome",
      ReadAloudService::OverviewOutcome::kGenerationFailed, 1);
  histograms.ExpectTotalCount("ReadAloud.Overview.TotalDuration", 0);
}

TEST_F(ReadAloudServiceTest, OverviewModeSendsPlayBeforeOverviewContent) {
  dom_distiller::ViewRequestDelegate* view_delegate = StartOverviewSession();
  ASSERT_NE(view_delegate, nullptr);
  service()->Play(web_contents());

  DeliverDistilledArticle(view_delegate);

  // Play() must reach the utility before SetOverviewContent() so the utility
  // starts playback when the script loads instead of reporting kPaused.
  EXPECT_EQ(fake_controller()->play_count_at_set_overview_content(), 1);
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

TEST_F(ReadAloudServiceTest, InitVoices) {
  MockDelegate* delegate_ptr = SetUpMockDelegate();

  EXPECT_CALL(*delegate_ptr, OnVoicesAvailable(testing::_))
      .WillOnce([](base::span<const VoiceInfo> voices) {
        ASSERT_EQ(voices.size(), std::size(kAvailableVoices));
        EXPECT_EQ(voices[0].id, kAvailableVoices[0].id);
        EXPECT_EQ(voices[0].display_name, kAvailableVoices[0].display_name);
        EXPECT_EQ(voices[0].lang, kAvailableVoices[0].lang);
        EXPECT_EQ(voices[0].region, kAvailableVoices[0].region);
        EXPECT_EQ(voices[0].pitch, kAvailableVoices[0].pitch);
        EXPECT_EQ(voices[0].tone, kAvailableVoices[0].tone);
      });
  service()->InitVoices();
}

TEST_F(ReadAloudServiceTest,
       RequestSpeechSynthesisDelegatesToBrokerAndHandlesError) {
  bool callback_called = false;
  service()->RequestSpeechSynthesis(
      /*text_chunk=*/u"Hello world",
      /*speaker=*/read_aloud::mojom::Speaker::kSpeaker1, /*sequence_id=*/1,
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

TEST_F(ReadAloudServiceTest, SetVoiceUpdatesSynthesizeRequestVoiceId) {
  auto* mock_opt_guide = static_cast<MockOptimizationGuideKeyedService*>(
      OptimizationGuideKeyedServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating([](content::BrowserContext*)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<
                    testing::NiceMock<MockOptimizationGuideKeyedService>>();
              })));

  service()->SetVoice("custom-voice-id");

  optimization_guide::proto::Any any;
  any.set_value("fake_audio_bytes");

  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& synthesize_request = static_cast<
                const optimization_guide::proto::ReadAloudSynthesizeRequest&>(
                request_metadata);
            EXPECT_EQ(synthesize_request.voice_id(), "custom-voice-id");
            EXPECT_EQ(synthesize_request.text_chunk(), "Hello world");

            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  base::test::TestFuture<mojo_base::BigBuffer, bool> future;
  service()->RequestSpeechSynthesis(
      /*text_chunk=*/u"Hello world",
      /*speaker=*/read_aloud::mojom::Speaker::kSpeaker1, /*sequence_id=*/1,
      future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(base::as_string_view(response_bytes), "fake_audio_bytes");
}

TEST_F(ReadAloudServiceTest,
       RequestSpeechSynthesisOverviewModeSpeaker1AndSpeaker2) {
  MockOptimizationGuideKeyedService* mock_opt_guide =
      SetUpMockOptimizationGuide();

  service()->SetPlaybackMode(ReadAloudService::PlaybackMode::kOverview);

  optimization_guide::proto::Any any;
  any.set_value("fake_audio_bytes");

  // Verify that speaker 1 and speaker 2 use their respective overview voice IDs.
  const struct {
    read_aloud::mojom::Speaker speaker;
    std::string_view expected_voice;
    const char16_t* text;
  } kTestCases[] = {
      {read_aloud::mojom::Speaker::kSpeaker1,
       SpeechSynthesisBroker::kOverviewVoiceSpeaker1, u"Speaker 1 line"},
      {read_aloud::mojom::Speaker::kSpeaker2,
       SpeechSynthesisBroker::kOverviewVoiceSpeaker2, u"Speaker 2 line"},
  };

  int sequence_id = 1;
  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(
        *mock_opt_guide,
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
            testing::_, testing::_, testing::_))
        .WillOnce(
            [&any, expected_voice = test_case.expected_voice](
                optimization_guide::ModelBasedCapabilityKey feature,
                const google::protobuf::MessageLite& request_metadata,
                const optimization_guide::ModelExecutionOptions& options,
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              const auto& req = static_cast<
                  const optimization_guide::proto::ReadAloudSynthesizeRequest&>(
                  request_metadata);
              EXPECT_EQ(req.voice_id(), expected_voice);
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      any, /*execution_info=*/nullptr),
                  /*log_entry=*/nullptr);
            });

    base::test::TestFuture<mojo_base::BigBuffer, bool> future;
    service()->RequestSpeechSynthesis(test_case.text, test_case.speaker,
                                       sequence_id++, future.GetCallback());
    EXPECT_TRUE(future.Get<bool>());
  }
}

TEST_F(ReadAloudServiceTest, OnTextChunkedForwardsToDelegate) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  std::vector<std::u16string> chunks = {u"First chunk.", u"Second chunk!"};

  EXPECT_CALL(*delegate_ptr, OnTextChunked(chunks)).Times(1);
  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);

  service()->OnTextChunked(chunks);
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedForwardsHighlightInBounds) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  // Highlight space: "First chunk." (12) + 1 + "Second chunk!" (13) = 26.
  service()->OnTextChunked({u"First chunk.", u"Second chunk!"});

  // "Second" starts right after "First chunk." plus one separator.
  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated(/*absolute_start_index=*/13,
                                                    /*absolute_end_index=*/19));

  service()->OnWordBoundaryReached(/*start_character_offset=*/13,
                                   /*end_character_offset=*/19);
}

TEST_F(ReadAloudServiceTest,
       OnWordBoundaryReachedForwardsHighlightEndingAtTextLength) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  service()->OnTextChunked({u"Hello."});  // Highlight space length 6.

  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated(/*absolute_start_index=*/0,
                                                    /*absolute_end_index=*/6));

  service()->OnWordBoundaryReached(/*start_character_offset=*/0,
                                   /*end_character_offset=*/6);
}

TEST_F(ReadAloudServiceTest,
       OnWordBoundaryReachedDropsHighlightEndingAfterLastChunk) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  service()->OnTextChunked({u"Hello."});  // Highlight space length 6.

  // No word can end in the trailing separator, so 7 is out of range.
  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated).Times(0);

  service()->OnWordBoundaryReached(/*start_character_offset=*/0,
                                   /*end_character_offset=*/7);
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedDropsOutOfBoundsHighlight) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  service()->OnTextChunked({u"Hello."});  // Highlight space length 6.

  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated).Times(0);

  service()->OnWordBoundaryReached(/*start_character_offset=*/3,
                                   /*end_character_offset=*/8);
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedDropsEmptyHighlight) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  service()->OnTextChunked({u"Hello."});

  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated).Times(0);

  service()->OnWordBoundaryReached(/*start_character_offset=*/2,
                                   /*end_character_offset=*/2);
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedDropsHighlightAfterStop) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));
  service()->OnTextChunked({u"Hello."});
  service()->Stop();

  EXPECT_CALL(*delegate_ptr, OnWordHighlightUpdated).Times(0);

  service()->OnWordBoundaryReached(/*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
}

TEST_F(ReadAloudServiceTest, OnTextChunkedExceedsLimit) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  std::vector<std::u16string> chunks(readaloud::kMaxTextChunks + 1, u"chunk");
  fake_controller()->client()->OnTextChunked(chunks);

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Received invalid chunk payload");
}

TEST_F(ReadAloudServiceTest, PlayDoesNotEmitPlayingDirectly) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // The utility process is authoritative for kPlaying. With no utility
  // response wired up, Play() must not fabricate the state on its own.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kPlaying))
      .Times(0);

  service()->Play(web_contents());
  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, PauseDoesNotEmitPausedDirectly) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());
  service()->Play(web_contents());

  // The utility process is authoritative for kPaused when bound and active.
  // With no utility response wired up, Pause() must not fabricate the state on
  // its own.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(0);

  service()->Pause();
  fake_controller()->FlushForTesting();
  EXPECT_EQ(fake_controller()->pause_count(), 1);

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, UtilityPlayingResponseToPlayReachesDelegate) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // Play() reaches the utility controller, which answers with kPlaying.
  fake_controller()->set_play_callback(base::BindLambdaForTesting([&]() {
    fake_controller()->client()->OnPlaybackStateChanged(
        read_aloud::mojom::PlaybackState::kPlaying);
  }));

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);

  service()->Play(web_contents());
  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, UtilityPlaybackStatesReachDelegate) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  testing::InSequence s;

  // Simulate utility process sending states.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kBuffering);

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);

  EXPECT_CALL(
      *delegate_ptr,
      OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kEndOfStream))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kEndOfStream);

  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();
  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, UtilityErrorRoutesToOnPlaybackError) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // An error state from utility should call OnPlaybackError and NOT emit a
  // UI state.
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kError))
      .Times(0);

  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);

  EXPECT_CALL(*delegate_ptr,
              OnPlaybackError("Playback error reported by utility process"))
      .Times(1);

  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kError);

  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, UtilityStatesAreForwardedWithoutFiltering) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // The browser forwards every utility state; duplicates are handled by the
  // utility process and the Java UI.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kPlaying))
      .Times(2);

  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);

  fake_controller()->FlushForTesting();

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, ShutdownDoesNotEmitStopped) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  ExpectInitializeCallbacks(delegate_ptr);
  service()->Initialize(web_contents());

  // Shutdown() tears down via OnNativeDestroyed() rather than kStopped.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(0);
  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, ReinitializeDoesNotEmitStopped) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* delegate_ptr = delegate.get();
  service()->SetDelegate(std::move(delegate));

  // Initializing twice resets the previous session and goes straight to
  // kPlaybackCreation, without an intermediate kStopped.
  EXPECT_CALL(*delegate_ptr, OnPlaybackStateChanged(
                                 read_aloud::mojom::PlaybackState::kStopped))
      .Times(0);
  EXPECT_CALL(*delegate_ptr,
              OnPlaybackStateChanged(
                  read_aloud::mojom::PlaybackState::kPlaybackCreation))
      .Times(2);

  service()->Initialize(web_contents());
  service()->Initialize(web_contents());

  EXPECT_CALL(*delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, NewDelegateReceivesStoppedAfterPreviousStop) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());

  auto first_delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* first_delegate_ptr = first_delegate.get();
  service()->SetDelegate(std::move(first_delegate));

  ExpectInitializeCallbacks(first_delegate_ptr);
  service()->Initialize(web_contents());

  EXPECT_CALL(
      *first_delegate_ptr,
      OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  // A newly attached delegate must not be suppressed by state that was only
  // delivered to the previous delegate.
  auto second_delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate* second_delegate_ptr = second_delegate.get();
  service()->SetDelegate(std::move(second_delegate));

  EXPECT_CALL(
      *second_delegate_ptr,
      OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  service()->Stop();

  EXPECT_CALL(*second_delegate_ptr, OnNativeDestroyed()).Times(1);
}

TEST_F(ReadAloudServiceTest, OnPlaybackStateChangedRejectsStopped) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kStopped);

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: browser-only PlaybackState received from utility");
}

TEST_F(ReadAloudServiceTest, OnPlaybackStateChangedRejectsPlaybackCreation) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaybackCreation);

  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudService: browser-only PlaybackState received from utility");
}

TEST_F(ReadAloudServiceTest, OnWordBoundaryReachedRejectsStartAfterEnd) {
  NavigateAndCommit(GURL("https://www.example.com/article"));
  SetFakeController(std::make_unique<FakePlaybackController>());
  service()->Initialize(web_contents());

  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnWordBoundaryReached(
      /*start_character_offset=*/5, /*end_character_offset=*/4);

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudService: word boundary start exceeds end");
}

}  // namespace readaloud
