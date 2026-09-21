// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Stands in for the TtsService normally hosted in the TTS utility process and
// records the AudioStreamFactory remote the browser passes to it.
class StubTtsService : public chromeos::tts::mojom::TtsService {
 public:
  StubTtsService() = default;
  StubTtsService(const StubTtsService&) = delete;
  StubTtsService& operator=(const StubTtsService&) = delete;
  ~StubTtsService() override = default;

  void Bind(mojo::PendingReceiver<chromeos::tts::mojom::TtsService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void WaitForStreamFactory() {
    if (stream_factory_.is_bound()) {
      return;
    }
    base::RunLoop loop;
    stream_factory_bound_closure_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Remote<media::mojom::AudioStreamFactory>& stream_factory() {
    return stream_factory_;
  }

  // chromeos::tts::mojom::TtsService:
  void BindGoogleTtsStream(
      mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory)
      override {
    google_tts_stream_receiver_ = std::move(receiver);
    OnStreamFactory(std::move(stream_factory));
  }

  void BindPlaybackTtsStream(
      mojo::PendingReceiver<chromeos::tts::mojom::PlaybackTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      chromeos::tts::mojom::AudioParametersPtr desired_audio_parameters,
      BindPlaybackTtsStreamCallback callback) override {
    playback_tts_stream_receiver_ = std::move(receiver);
    std::move(callback).Run(chromeos::tts::mojom::AudioParameters::New(
        /*sample_rate=*/22050, /*buffer_size=*/512));
    OnStreamFactory(std::move(stream_factory));
  }

 private:
  void OnStreamFactory(
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory) {
    stream_factory_.Bind(std::move(stream_factory));
    if (stream_factory_bound_closure_) {
      std::move(stream_factory_bound_closure_).Run();
    }
  }

  mojo::Receiver<chromeos::tts::mojom::TtsService> receiver_{this};
  mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream>
      google_tts_stream_receiver_;
  mojo::PendingReceiver<chromeos::tts::mojom::PlaybackTtsStream>
      playback_tts_stream_receiver_;
  mojo::Remote<media::mojom::AudioStreamFactory> stream_factory_;
  base::OnceClosure stream_factory_bound_closure_;
};

// AudioInputStreamClient that keeps the client endpoint alive, as a real
// capture client would.
class StubAudioInputStreamClient : public media::mojom::AudioInputStreamClient {
 public:
  mojo::PendingRemote<media::mojom::AudioInputStreamClient> MakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // media::mojom::AudioInputStreamClient:
  void OnError(media::mojom::InputStreamErrorCode code) override {}
  void OnMutedStateChanged(bool is_muted) override {}

 private:
  mojo::Receiver<media::mojom::AudioInputStreamClient> receiver_{this};
};

class TtsEngineExtensionObserverChromeOSBrowserTest
    : public InProcessBrowserTest {
 public:
  TtsEngineExtensionObserverChromeOS* GetObserver() {
    return TtsEngineExtensionObserverChromeOSFactory::GetForProfile(
        GetProfile());
  }

  // Routes the observer's TtsService connection to |stub_service_| and asks
  // the observer to bind a playback tts stream, then returns the audio stream
  // factory the observer handed across.
  mojo::Remote<media::mojom::AudioStreamFactory>& BindPlaybackStreamFactory() {
    TtsEngineExtensionObserverChromeOS* observer = GetObserver();
    stub_service_.Bind(
        observer->tts_service_for_testing()->BindNewPipeAndPassReceiver());
    observer->BindPlaybackTtsStream(
        playback_tts_stream_.BindNewPipeAndPassReceiver(),
        /*audio_parameters=*/nullptr, base::DoNothing());
    stub_service_.WaitForStreamFactory();
    return stub_service_.stream_factory();
  }

  // Same as above for the google tts stream.
  mojo::Remote<media::mojom::AudioStreamFactory>& BindGoogleStreamFactory() {
    TtsEngineExtensionObserverChromeOS* observer = GetObserver();
    stub_service_.Bind(
        observer->tts_service_for_testing()->BindNewPipeAndPassReceiver());
    observer->BindGoogleTtsStream(
        google_tts_stream_.BindNewPipeAndPassReceiver());
    stub_service_.WaitForStreamFactory();
    return stub_service_.stream_factory();
  }

 private:
  StubTtsService stub_service_;
  mojo::Remote<chromeos::tts::mojom::PlaybackTtsStream> playback_tts_stream_;
  mojo::Remote<chromeos::tts::mojom::GoogleTtsStream> google_tts_stream_;
};

// The factory handed to the TTS service must create output streams for
// speech playback; the request has to reach the audio service and produce a
// response while the factory connection stays open.
IN_PROC_BROWSER_TEST_F(TtsEngineExtensionObserverChromeOSBrowserTest,
                       PlaybackTtsStreamFactoryCreatesOutputStream) {
  mojo::Remote<media::mojom::AudioStreamFactory>& factory =
      BindPlaybackStreamFactory();

  base::RunLoop loop;
  bool got_response = false;
  mojo::Remote<media::mojom::AudioOutputStream> stream;
  factory.set_disconnect_handler(loop.QuitClosure());
  factory->CreateOutputStream(
      stream.BindNewPipeAndPassReceiver(), mojo::NullAssociatedRemote(),
      mojo::NullRemote(), media::AudioDeviceDescription::kDefaultDeviceId,
      media::AudioParameters::UnavailableDeviceParams(),
      base::UnguessableToken::Create(),
      base::BindLambdaForTesting(
          [&](media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
            got_response = true;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(got_response);
  EXPECT_TRUE(factory.is_connected());
}

// The TTS service only plays audio; a request for an audio input stream on
// the factory it was handed must never produce a capture data pipe.
IN_PROC_BROWSER_TEST_F(TtsEngineExtensionObserverChromeOSBrowserTest,
                       PlaybackTtsStreamFactoryRejectsInputStream) {
  mojo::Remote<media::mojom::AudioStreamFactory>& factory =
      BindPlaybackStreamFactory();

  base::RunLoop loop;
  media::mojom::ReadWriteAudioDataPipePtr received_data_pipe;
  bool got_response = false;
  StubAudioInputStreamClient client;
  mojo::Remote<media::mojom::AudioInputStream> stream;
  factory->CreateInputStream(
      stream.BindNewPipeAndPassReceiver(), client.MakeRemote(),
      mojo::NullRemote(), mojo::NullRemote(),
      media::AudioDeviceDescription::kDefaultDeviceId,
      media::AudioParameters::UnavailableDeviceParams(),
      base::UnguessableToken::Create(), /*shared_memory_count=*/10,
      /*enable_agc=*/false, /*processing_config=*/nullptr,
      base::BindLambdaForTesting(
          [&](media::mojom::ReadWriteAudioDataPipePtr data_pipe,
              bool initially_muted,
              const std::optional<base::UnguessableToken>& stream_id) {
            got_response = true;
            received_data_pipe = std::move(data_pipe);
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(got_response);
  EXPECT_FALSE(received_data_pipe);
}

// As above, but for loopback capture of system audio output.
IN_PROC_BROWSER_TEST_F(TtsEngineExtensionObserverChromeOSBrowserTest,
                       PlaybackTtsStreamFactoryRejectsLoopbackStream) {
  mojo::Remote<media::mojom::AudioStreamFactory>& factory =
      BindPlaybackStreamFactory();

  base::RunLoop loop;
  media::mojom::ReadWriteAudioDataPipePtr received_data_pipe;
  bool got_response = false;
  StubAudioInputStreamClient client;
  mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer;
  std::ignore = observer.InitWithNewPipeAndPassReceiver();
  mojo::Remote<media::mojom::AudioInputStream> stream;
  factory->CreateLoopbackStream(
      stream.BindNewPipeAndPassReceiver(), client.MakeRemote(),
      std::move(observer), media::AudioParameters::UnavailableDeviceParams(),
      /*shared_memory_count=*/10, base::UnguessableToken::Create(),
      base::BindLambdaForTesting(
          [&](media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
            got_response = true;
            received_data_pipe = std::move(data_pipe);
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(got_response);
  EXPECT_FALSE(received_data_pipe);
}

// The google tts stream is bound with its own factory; it must be subject to
// the same output-only contract.
IN_PROC_BROWSER_TEST_F(TtsEngineExtensionObserverChromeOSBrowserTest,
                       GoogleTtsStreamFactoryRejectsInputStream) {
  mojo::Remote<media::mojom::AudioStreamFactory>& factory =
      BindGoogleStreamFactory();

  base::RunLoop loop;
  media::mojom::ReadWriteAudioDataPipePtr received_data_pipe;
  bool got_response = false;
  StubAudioInputStreamClient client;
  mojo::Remote<media::mojom::AudioInputStream> stream;
  factory->CreateInputStream(
      stream.BindNewPipeAndPassReceiver(), client.MakeRemote(),
      mojo::NullRemote(), mojo::NullRemote(),
      media::AudioDeviceDescription::kDefaultDeviceId,
      media::AudioParameters::UnavailableDeviceParams(),
      base::UnguessableToken::Create(), /*shared_memory_count=*/10,
      /*enable_agc=*/false, /*processing_config=*/nullptr,
      base::BindLambdaForTesting(
          [&](media::mojom::ReadWriteAudioDataPipePtr data_pipe,
              bool initially_muted,
              const std::optional<base::UnguessableToken>& stream_id) {
            got_response = true;
            received_data_pipe = std::move(data_pipe);
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(got_response);
  EXPECT_FALSE(received_data_pipe);
}

}  // namespace
