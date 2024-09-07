// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/media_router/common/pref_names.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video_capture_types.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace mirroring {

namespace {

media::VideoCaptureParams DefaultVideoCaptureParams() {
  constexpr gfx::Size kMaxCaptureSize = gfx::Size(320, 320);
  constexpr int kMaxFramesPerSecond = 60;
  gfx::Size capture_size = kMaxCaptureSize;
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      capture_size, kMaxFramesPerSecond, media::PIXEL_FORMAT_I420);
  params.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  return params;
}

content::DesktopMediaID BuildMediaIdForTabMirroring(
    content::WebContents* target_web_contents) {
  DCHECK(target_web_contents);
  content::DesktopMediaID media_id;
  content::RenderFrameHost* const main_frame =
      target_web_contents->GetPrimaryMainFrame();
  const int process_id = main_frame->GetProcess()->GetID();
  const int frame_id = main_frame->GetRoutingID();
  media_id.type = content::DesktopMediaID::TYPE_WEB_CONTENTS;
  media_id.web_contents_id = content::WebContentsMediaCaptureId(
      process_id, frame_id, true /* disable_local_echo */);
  return media_id;
}

class MockVideoCaptureObserver final
    : public media::mojom::VideoCaptureObserver {
 public:
  explicit MockVideoCaptureObserver(
      mojo::PendingRemote<media::mojom::VideoCaptureHost> host)
      : host_(std::move(host)) {}

  MockVideoCaptureObserver(const MockVideoCaptureObserver&) = delete;
  MockVideoCaptureObserver& operator=(const MockVideoCaptureObserver&) = delete;

  MOCK_METHOD1(OnBufferCreatedCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferReadyCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferDestroyedCall, void(int buffer_id));
  MOCK_METHOD1(OnStateChangedCall, void(media::mojom::VideoCaptureState state));
  MOCK_METHOD1(OnVideoCaptureErrorCall, void(media::VideoCaptureError error));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason reason));

  // media::mojom::VideoCaptureObserver implementation.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    EXPECT_EQ(buffers_.find(buffer_id), buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    buffers_[buffer_id] = std::move(buffer_handle);
    OnBufferCreatedCall(buffer_id);
  }

  void OnBufferReady(media::mojom::ReadyBufferPtr buffer) override {
    EXPECT_TRUE(buffers_.find(buffer->buffer_id) != buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer->buffer_id), frame_infos_.end());
    frame_infos_[buffer->buffer_id] = std::move(buffer->info);
    OnBufferReadyCall(buffer->buffer_id);
  }

  void OnBufferDestroyed(int32_t buffer_id) override {
    // The consumer should have finished consuming the buffer before it is being
    // destroyed.
    EXPECT_TRUE(frame_infos_.find(buffer_id) == frame_infos_.end());
    const auto iter = buffers_.find(buffer_id);
    EXPECT_TRUE(iter != buffers_.end());
    buffers_.erase(iter);
    OnBufferDestroyedCall(buffer_id);
  }

  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override {}

  void OnStateChanged(media::mojom::VideoCaptureResultPtr result) override {
    if (result->which() == media::mojom::VideoCaptureResult::Tag::kState)
      OnStateChangedCall(result->get_state());
    else
      OnVideoCaptureErrorCall(result->get_error_code());
  }

  void Start() {
    host_->Start(device_id_, session_id_, DefaultVideoCaptureParams(),
                 receiver_.BindNewPipeAndPassRemote());
  }

  void Stop() { host_->Stop(device_id_); }

  void RequestRefreshFrame() { host_->RequestRefreshFrame(device_id_); }

 private:
  mojo::Remote<media::mojom::VideoCaptureHost> host_;
  mojo::Receiver<media::mojom::VideoCaptureObserver> receiver_{this};
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;
  base::flat_map<int, media::mojom::VideoFrameInfoPtr> frame_infos_;
  const base::UnguessableToken device_id_ = base::UnguessableToken::Create();
  const base::UnguessableToken session_id_ = base::UnguessableToken::Create();
};

class MockCastMessageChannel : public mojom::CastMessageChannel {
 public:
  // mojom::CastMessageChannel mock implementation (outbound messages).
  MOCK_METHOD(void, OnMessage, (mojom::CastMessagePtr));

  mojo::Receiver<mojom::CastMessageChannel>* GetChannelReceiver() {
    return &channel_receiver_;
  }

 private:
  mojo::Receiver<mojom::CastMessageChannel> channel_receiver_{this};
};

}  // namespace

class CastMirroringServiceHostBrowserTest
    : public InProcessBrowserTest,
      public mojom::SessionObserver,
      public mojom::CastMessageChannel,
      public mojom::AudioStreamCreatorClient {
 public:
  CastMirroringServiceHostBrowserTest() = default;

  CastMirroringServiceHostBrowserTest(
      const CastMirroringServiceHostBrowserTest&) = delete;
  CastMirroringServiceHostBrowserTest& operator=(
      const CastMirroringServiceHostBrowserTest&) = delete;

  ~CastMirroringServiceHostBrowserTest() override = default;

 protected:
  // Starts a tab mirroring session.
  void StartTabMirroring() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    host_ = std::make_unique<CastMirroringServiceHost>(
        BuildMediaIdForTabMirroring(web_contents));
    mojo::PendingRemote<mojom::SessionObserver> observer;
    observer_receiver_.Bind(observer.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel;
    outbound_channel_receiver_ = std::make_unique<MockCastMessageChannel>();
    outbound_channel_receiver_->GetChannelReceiver()->Bind(
        outbound_channel.InitWithNewPipeAndPassReceiver());
    auto session_params = mojom::SessionParameters::New();
    session_params->source_id = "SourceID";
    host_->Start(std::move(session_params), std::move(observer),
                 std::move(outbound_channel),
                 inbound_channel_.BindNewPipeAndPassReceiver(), "Sink Name");
  }

  // Starts a tab mirroring session, and sets a target playout delay.
  // `media_source_delay` simulates the mirroring delay that is set from a media
  // source when starting a mirroring session, i.e. set by a site-initiated
  // mirroring source. `feature_delay` is the value that media_router_feature's
  // `GetCastMirroringPlayoutDelay` is expected to return.
  void StartTabMirroringWithTargetPlayoutDelay(
      base::TimeDelta media_source_delay) {
    int expected_delay_ms = media_source_delay.InMilliseconds();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    host_ = std::make_unique<CastMirroringServiceHost>(
        BuildMediaIdForTabMirroring(web_contents));
    mojo::PendingRemote<mojom::SessionObserver> observer;
    observer_receiver_.Bind(observer.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel;
    outbound_channel_receiver_ = std::make_unique<MockCastMessageChannel>();
    outbound_channel_receiver_->GetChannelReceiver()->Bind(
        outbound_channel.InitWithNewPipeAndPassReceiver());
    auto session_params = mojom::SessionParameters::New();
    session_params->source_id = "SourceID";
    session_params->target_playout_delay = media_source_delay;

    base::RunLoop run_loop;
    EXPECT_CALL(*outbound_channel_receiver_, OnMessage(_))
        .WillOnce(
            testing::Invoke([expected_delay_ms, &run_loop](
                                mirroring::mojom::CastMessagePtr message) {
              const std::optional<base::Value> root_or_error =
                  base::JSONReader::Read(message->json_format_data);
              ASSERT_TRUE(root_or_error);
              const base::Value::Dict& root = root_or_error->GetDict();
              const std::string* type = root.FindString("type");
              ASSERT_TRUE(type);
              if (*type == "OFFER") {
                const base::Value::Dict* offer = root.FindDict("offer");
                EXPECT_TRUE(offer);
                const base::Value::List* streams =
                    offer->FindList("supportedStreams");
                for (auto& stream : *streams) {
                  const base::Value::Dict& stream_dict = stream.GetDict();
                  const int stream_target_delay =
                      stream_dict.FindInt("targetDelay").value();
                  EXPECT_EQ(stream_target_delay, expected_delay_ms);
                }
              }
              run_loop.Quit();
            }));
    host_->Start(std::move(session_params), std::move(observer),
                 std::move(outbound_channel),
                 inbound_channel_.BindNewPipeAndPassReceiver(), "Sink Name");
    run_loop.Run();
  }

  void EnableAccessCodeCast() {
    ASSERT_FALSE(media_router::GetAccessCodeCastEnabledPref(
        browser()->tab_strip_model()->profile()));
    browser()->tab_strip_model()->profile()->GetPrefs()->SetBoolean(
        media_router::prefs::kAccessCodeCastEnabled, true);
    ASSERT_TRUE(media_router::GetAccessCodeCastEnabledPref(
        browser()->tab_strip_model()->profile()));
  }

  void SwitchTabSource() {
    ASSERT_TRUE(host_->tab_switching_ui_enabled_);
    browser()->tab_strip_model()->delegate()->AddTabAt(GURL(), -1, true);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::FrameTreeNodeId web_contents_source_tab_id =
        web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId();

    ASSERT_NE(host_->GetTabSourceId(), web_contents_source_tab_id);
    ASSERT_NE(host_->web_contents(), web_contents);
    host_->SwitchMirroringSourceTab(BuildMediaIdForTabMirroring(web_contents),
                                    /*captured_surface_control_active=*/false);
    ASSERT_EQ(host_->web_contents(), web_contents);
    ASSERT_EQ(host_->GetTabSourceId(), web_contents_source_tab_id);
  }

  void GetVideoCaptureHost() {
    mojo::PendingRemote<media::mojom::VideoCaptureHost> video_capture_host;
    static_cast<mojom::ResourceProvider*>(host_.get())
        ->GetVideoCaptureHost(
            video_capture_host.InitWithNewPipeAndPassReceiver());
    video_frame_receiver_ = std::make_unique<MockVideoCaptureObserver>(
        std::move(video_capture_host));
  }

  void StartVideoCapturing() {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_frame_receiver_,
                OnStateChangedCall(media::mojom::VideoCaptureState::STARTED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_frame_receiver_->Start();
    run_loop.Run();
  }

  void PauseMirroring() {
    base::RunLoop run_loop;
    host_->Pause(run_loop.QuitClosure());
    run_loop.Run();
  }

  void ResumeMirroring() {
    base::RunLoop run_loop;
    host_->Resume(run_loop.QuitClosure());
    run_loop.Run();
  }

  void StopMirroring() {
    if (video_frame_receiver_) {
      base::RunLoop run_loop;
      EXPECT_CALL(*video_frame_receiver_,
                  OnStateChangedCall(media::mojom::VideoCaptureState::ENDED))
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
      video_frame_receiver_->Stop();
      run_loop.Run();
    }
    host_.reset();
  }

  void RequestRefreshFrame() {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_frame_receiver_, OnBufferReadyCall(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit))
        .WillRepeatedly(Return());
    video_frame_receiver_->RequestRefreshFrame();
    run_loop.Run();
  }

  void CreateAudioLoopbackStream() {
    constexpr int kTotalSegments = 1;
    constexpr int kAudioTimebase = 48000;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::ChannelLayoutConfig::Stereo(),
                                  kAudioTimebase, kAudioTimebase / 100);
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnAudioStreamCreated())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    host_->CreateAudioStream(audio_client_receiver_.BindNewPipeAndPassRemote(),
                             params, kTotalSegments);
    run_loop.Run();
  }

  // InProcessBrowserTest override.
  void SetUp() override { InProcessBrowserTest::SetUp(); }

 private:
  // mojom::SessionObserver mocks.
  MOCK_METHOD(void, OnError, (mojom::SessionError));
  MOCK_METHOD(void, DidStart, ());
  MOCK_METHOD(void, DidStop, ());
  MOCK_METHOD(void, LogInfoMessage, (const std::string&));
  MOCK_METHOD(void, LogErrorMessage, (const std::string&));
  MOCK_METHOD(void, OnSourceChanged, ());
  MOCK_METHOD(void, OnRemotingStateChanged, (bool is_remoting));

  // mojom::CastMessageChannel mock implementation (inbound messages).
  MOCK_METHOD(void, OnMessage, (mojom::CastMessagePtr));

  // mojom::AudioStreamCreatorClient mocks.
  MOCK_METHOD(void, OnAudioStreamCreated, ());
  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe) override {
    EXPECT_TRUE(stream);
    EXPECT_TRUE(client_receiver);
    EXPECT_TRUE(data_pipe);
    OnAudioStreamCreated();
  }

  mojo::Receiver<mojom::SessionObserver> observer_receiver_{this};
  mojo::Receiver<mojom::AudioStreamCreatorClient> audio_client_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;

  std::unique_ptr<CastMirroringServiceHost> host_;
  std::unique_ptr<MockVideoCaptureObserver> video_frame_receiver_;
  std::unique_ptr<MockCastMessageChannel> outbound_channel_receiver_;
};

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest, CaptureTabVideo) {
  StartTabMirroring();
  GetVideoCaptureHost();
  StartVideoCapturing();
  RequestRefreshFrame();
  StopMirroring();
}

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest, CaptureTabAudio) {
  StartTabMirroring();
  CreateAudioLoopbackStream();
  StopMirroring();
}

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest, TabIndicator) {
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(GetTabAlertStatesForContents(contents), ::testing::IsEmpty());

  // A TabStripModelObserver that quits the MessageLoop whenever the
  // UI's model is sent an event that might change the indicator status.
  class IndicatorChangeObserver : public TabStripModelObserver {
   public:
    explicit IndicatorChangeObserver(Browser* browser) : browser_(browser) {
      browser_->tab_strip_model()->AddObserver(this);
    }

    void TabChangedAt(content::WebContents* contents,
                      int index,
                      TabChangeType change_type) override {
      std::move(on_tab_changed_).Run();
    }

    void WaitForTabChange() {
      base::RunLoop run_loop;
      on_tab_changed_ = run_loop.QuitClosure();
      run_loop.Run();
    }

   private:
    const raw_ptr<Browser> browser_;
    base::OnceClosure on_tab_changed_;
  };

  IndicatorChangeObserver observer(browser());
  ASSERT_THAT(GetTabAlertStatesForContents(contents), ::testing::IsEmpty());
  StartTabMirroring();

  // Run the browser until the indicator turns on.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!base::Contains(GetTabAlertStatesForContents(contents),
                         TabAlertState::TAB_CAPTURING)) {
    if (base::TimeTicks::Now() - start_time >
        TestTimeouts::action_max_timeout()) {
      EXPECT_THAT(GetTabAlertStatesForContents(contents),
                  ::testing::Contains(TabAlertState::TAB_CAPTURING));
      return;
    }
    observer.WaitForTabChange();
  }
  StopMirroring();
}

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest, PauseSession) {
  EnableAccessCodeCast();
  StartTabMirroring();
  GetVideoCaptureHost();
  StartVideoCapturing();
  PauseMirroring();
  ResumeMirroring();
  StopMirroring();
}

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest,
                       TabMirrorWithPresetPlayoutDelay) {
  StartTabMirroringWithTargetPlayoutDelay(base::Milliseconds(200));
  GetVideoCaptureHost();
  StartVideoCapturing();
  RequestRefreshFrame();
  StopMirroring();
}

class CastMirroringServiceHostBrowserTestTabSwitcher
    : public CastMirroringServiceHostBrowserTest {
 public:
  CastMirroringServiceHostBrowserTestTabSwitcher() {
    feature_list_.InitWithFeatures({features::kAccessCodeCastTabSwitchingUI},
                                   {});
  }

  CastMirroringServiceHostBrowserTestTabSwitcher(
      const CastMirroringServiceHostBrowserTestTabSwitcher&) = delete;
  CastMirroringServiceHostBrowserTestTabSwitcher& operator=(
      const CastMirroringServiceHostBrowserTestTabSwitcher&) = delete;

  ~CastMirroringServiceHostBrowserTestTabSwitcher() override = default;

  void VerifyEnabledFeatures() {
    ASSERT_TRUE(
        base::FeatureList::IsEnabled(features::kAccessCodeCastTabSwitchingUI));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTestTabSwitcher,
                       SwitchTabSource) {
  VerifyEnabledFeatures();
  EnableAccessCodeCast();
  StartTabMirroring();
  GetVideoCaptureHost();
  StartVideoCapturing();
  SwitchTabSource();
  GetVideoCaptureHost();
  StartVideoCapturing();
  StopMirroring();
}

}  // namespace mirroring
