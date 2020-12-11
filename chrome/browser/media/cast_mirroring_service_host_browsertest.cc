// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "media/capture/mojom/video_capture.mojom.h"
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
      target_web_contents->GetMainFrame();
  const int process_id = main_frame->GetProcess()->GetID();
  const int frame_id = main_frame->GetRoutingID();
  media_id.type = content::DesktopMediaID::TYPE_WEB_CONTENTS;
  media_id.web_contents_id =
      content::WebContentsMediaCaptureId(process_id, frame_id, true, true);
  return media_id;
}

class MockVideoCaptureObserver final
    : public media::mojom::VideoCaptureObserver {
 public:
  explicit MockVideoCaptureObserver(
      mojo::PendingRemote<media::mojom::VideoCaptureHost> host)
      : host_(std::move(host)) {}
  MOCK_METHOD1(OnBufferCreatedCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferReadyCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferDestroyedCall, void(int buffer_id));
  MOCK_METHOD1(OnStateChanged, void(media::mojom::VideoCaptureState state));

  // media::mojom::VideoCaptureObserver implementation.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    EXPECT_EQ(buffers_.find(buffer_id), buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    buffers_[buffer_id] = std::move(buffer_handle);
    OnBufferCreatedCall(buffer_id);
  }

  void OnBufferReady(int32_t buffer_id,
                     media::mojom::VideoFrameInfoPtr info) override {
    EXPECT_TRUE(buffers_.find(buffer_id) != buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    frame_infos_[buffer_id] = std::move(info);
    OnBufferReadyCall(buffer_id);
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

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureObserver);
};

}  // namespace

class CastMirroringServiceHostBrowserTest
    : public InProcessBrowserTest,
      public mojom::SessionObserver,
      public mojom::CastMessageChannel,
      public mojom::AudioStreamCreatorClient {
 public:
  CastMirroringServiceHostBrowserTest() = default;
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
    outbound_channel_receiver_.Bind(
        outbound_channel.InitWithNewPipeAndPassReceiver());
    host_->Start(mojom::SessionParameters::New(), std::move(observer),
                 std::move(outbound_channel),
                 inbound_channel_.BindNewPipeAndPassReceiver());
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
                OnStateChanged(media::mojom::VideoCaptureState::STARTED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_frame_receiver_->Start();
    run_loop.Run();
  }

  void StopMirroring() {
    if (video_frame_receiver_) {
      base::RunLoop run_loop;
      EXPECT_CALL(*video_frame_receiver_,
                  OnStateChanged(media::mojom::VideoCaptureState::ENDED))
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
                                  media::CHANNEL_LAYOUT_STEREO, kAudioTimebase,
                                  kAudioTimebase / 100);
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
  MOCK_METHOD1(OnError, void(mojom::SessionError));
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());

  // mojom::CastMessageChannel mocks.
  MOCK_METHOD1(Send, void(mojom::CastMessagePtr));

  // mojom::AudioStreamCreatorClient mocks.
  MOCK_METHOD0(OnAudioStreamCreated, void());
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
  mojo::Receiver<mojom::CastMessageChannel> outbound_channel_receiver_{this};
  mojo::Receiver<mojom::AudioStreamCreatorClient> audio_client_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;

  std::unique_ptr<CastMirroringServiceHost> host_;
  std::unique_ptr<MockVideoCaptureObserver> video_frame_receiver_;

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHostBrowserTest);
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
  ASSERT_THAT(chrome::GetTabAlertStatesForContents(contents),
              ::testing::IsEmpty());

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
    Browser* const browser_;
    base::OnceClosure on_tab_changed_;
  };

  IndicatorChangeObserver observer(browser());
  ASSERT_THAT(chrome::GetTabAlertStatesForContents(contents),
              ::testing::IsEmpty());
  StartTabMirroring();

  // Run the browser until the indicator turns on.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!base::Contains(chrome::GetTabAlertStatesForContents(contents),
                         TabAlertState::TAB_CAPTURING)) {
    if (base::TimeTicks::Now() - start_time >
        TestTimeouts::action_max_timeout()) {
      EXPECT_THAT(chrome::GetTabAlertStatesForContents(contents),
                  ::testing::Contains(TabAlertState::TAB_CAPTURING));
      return;
    }
    observer.WaitForTabChange();
  }
  StopMirroring();
}

}  // namespace mirroring
