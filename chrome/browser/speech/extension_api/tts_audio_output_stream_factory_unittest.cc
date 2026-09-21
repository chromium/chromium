// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_audio_output_stream_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::tts {

namespace {

class MockAudioServiceStreamFactory : public audio::FakeStreamFactory {
 public:
  MockAudioServiceStreamFactory() = default;
  ~MockAudioServiceStreamFactory() override = default;

  MOCK_METHOD(
      void,
      CreateOutputStream,
      (mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
       mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
           observer,
       mojo::PendingRemote<media::mojom::AudioLog> log,
       const std::string& device_id,
       const media::AudioParameters& params,
       const base::UnguessableToken& group_id,
       CreateOutputStreamCallback callback),
      (override));
};

class BadMessageListener {
 public:
  explicit BadMessageListener(base::RepeatingClosure on_bad_message = {})
      : on_bad_message_(std::move(on_bad_message)) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BadMessageListener::OnBadMessage, base::Unretained(this)));
  }
  ~BadMessageListener() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
    if (on_bad_message_) {
      on_bad_message_.Run();
    }
  }

  const std::vector<std::string>& bad_messages() const { return bad_messages_; }

 private:
  base::RepeatingClosure on_bad_message_;
  std::vector<std::string> bad_messages_;
};

}  // namespace

class TtsAudioOutputStreamFactoryTest : public testing::Test {
 public:
  TtsAudioOutputStreamFactoryTest() = default;
  ~TtsAudioOutputStreamFactoryTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockAudioServiceStreamFactory mock_audio_service_;
};

TEST_F(TtsAudioOutputStreamFactoryTest, ForwardsCreateOutputStream) {
  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver;
  auto stream = stream_receiver.InitWithNewPipeAndPassRemote();

  const std::string kDeviceId = "test_device_id";
  const media::AudioParameters kParams(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  const base::UnguessableToken kGroupId = base::UnguessableToken::Create();

  base::RunLoop run_loop;
  EXPECT_CALL(mock_audio_service_,
              CreateOutputStream(testing::_, testing::_, testing::_, kDeviceId,
                                 testing::_, kGroupId, testing::_))
      .WillOnce(
          [&](mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
              mojo::PendingAssociatedRemote<
                  media::mojom::AudioOutputStreamObserver> observer,
              mojo::PendingRemote<media::mojom::AudioLog> log,
              const std::string& device_id,
              const media::AudioParameters& params,
              const base::UnguessableToken& group_id,
              media::mojom::AudioStreamFactory::CreateOutputStreamCallback
                  callback) {
            std::move(callback).Run(media::mojom::ReadWriteAudioDataPipePtr());
            run_loop.Quit();
          });

  factory->CreateOutputStream(
      std::move(stream_receiver), /*observer=*/mojo::NullAssociatedRemote(),
      /*log=*/mojo::NullRemote(), kDeviceId, kParams, kGroupId,
      base::BindOnce([](media::mojom::ReadWriteAudioDataPipePtr data_pipe) {}));

  run_loop.Run();
}

TEST_F(TtsAudioOutputStreamFactoryTest, RejectsCreateInputStream) {
  base::RunLoop run_loop;
  BadMessageListener bad_message_listener(run_loop.QuitClosure());

  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver;
  auto stream = stream_receiver.InitWithNewPipeAndPassRemote();

  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver;
  auto client = client_receiver.InitWithNewPipeAndPassRemote();

  const media::AudioParameters kParams(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 16000, 160);

  factory->CreateInputStream(
      std::move(stream_receiver), std::move(client),
      /*observer=*/mojo::NullRemote(), /*log=*/mojo::NullRemote(),
      /*device_id=*/"default", kParams, base::UnguessableToken::Create(),
      /*shared_memory_count=*/10, /*enable_agc=*/false,
      /*processing_config=*/nullptr,
      base::BindOnce(
          [](media::mojom::ReadWriteAudioDataPipePtr data_pipe,
             bool initially_muted,
             const std::optional<base::UnguessableToken>& stream_id) {}));

  run_loop.Run();
  ASSERT_FALSE(bad_message_listener.bad_messages().empty());
  EXPECT_NE(bad_message_listener.bad_messages()[0].find(
                "CreateInputStream is rejected"),
            std::string::npos);
}

TEST_F(TtsAudioOutputStreamFactoryTest, RejectsCreateLoopbackStream) {
  base::RunLoop run_loop;
  BadMessageListener bad_message_listener(run_loop.QuitClosure());

  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver;
  auto stream = stream_receiver.InitWithNewPipeAndPassRemote();

  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver;
  auto client = client_receiver.InitWithNewPipeAndPassRemote();

  mojo::PendingReceiver<media::mojom::AudioInputStreamObserver>
      observer_receiver;
  auto observer = observer_receiver.InitWithNewPipeAndPassRemote();

  const media::AudioParameters kParams(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);

  factory->CreateLoopbackStream(
      std::move(stream_receiver), std::move(client), std::move(observer),
      kParams, /*shared_memory_count=*/10, base::UnguessableToken::Create(),
      base::BindOnce([](media::mojom::ReadWriteAudioDataPipePtr data_pipe) {}));

  run_loop.Run();
  ASSERT_FALSE(bad_message_listener.bad_messages().empty());
  EXPECT_NE(bad_message_listener.bad_messages()[0].find(
                "CreateLoopbackStream is rejected"),
            std::string::npos);
}

TEST_F(TtsAudioOutputStreamFactoryTest, RejectsCreateSwitchableOutputStream) {
  base::RunLoop run_loop;
  BadMessageListener bad_message_listener(run_loop.QuitClosure());

  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver;
  auto stream = stream_receiver.InitWithNewPipeAndPassRemote();

  mojo::PendingReceiver<media::mojom::DeviceSwitchInterface> switch_receiver;
  auto switch_remote = switch_receiver.InitWithNewPipeAndPassRemote();

  const media::AudioParameters kParams(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);

  factory->CreateSwitchableOutputStream(
      std::move(stream_receiver), std::move(switch_receiver),
      /*observer=*/mojo::NullAssociatedRemote(), /*log=*/mojo::NullRemote(),
      /*device_id=*/"default", kParams, base::UnguessableToken::Create(),
      base::BindOnce([](media::mojom::ReadWriteAudioDataPipePtr data_pipe) {}));

  run_loop.Run();
  ASSERT_FALSE(bad_message_listener.bad_messages().empty());
  EXPECT_NE(bad_message_listener.bad_messages()[0].find(
                "CreateSwitchableOutputStream is not supported"),
            std::string::npos);
}

TEST_F(TtsAudioOutputStreamFactoryTest, RejectsAssociateInputAndOutputForAec) {
  base::RunLoop run_loop;
  BadMessageListener bad_message_listener(run_loop.QuitClosure());

  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  factory->AssociateInputAndOutputForAec(base::UnguessableToken::Create(),
                                         "output_device_id");

  run_loop.Run();
  ASSERT_FALSE(bad_message_listener.bad_messages().empty());
  EXPECT_NE(bad_message_listener.bad_messages()[0].find(
                "AssociateInputAndOutputForAec is rejected"),
            std::string::npos);
}

TEST_F(TtsAudioOutputStreamFactoryTest, RejectsBindMuter) {
  base::RunLoop run_loop;
  BadMessageListener bad_message_listener(run_loop.QuitClosure());

  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  mojo::AssociatedRemote<media::mojom::LocalMuter> muter_remote;
  auto muter_receiver = muter_remote.BindNewEndpointAndPassReceiver();

  factory->BindMuter(std::move(muter_receiver),
                     base::UnguessableToken::Create());

  run_loop.Run();
  ASSERT_FALSE(bad_message_listener.bad_messages().empty());
  EXPECT_NE(
      bad_message_listener.bad_messages()[0].find("BindMuter is rejected"),
      std::string::npos);
}

TEST_F(TtsAudioOutputStreamFactoryTest,
       DisconnectsWhenAudioServiceDisconnects) {
  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  base::RunLoop run_loop;
  factory.set_disconnect_handler(run_loop.QuitClosure());

  // Simulate audio service crashing or closing the stream factory pipe.
  mock_audio_service_.ResetReceiver();

  run_loop.Run();
  EXPECT_FALSE(factory.is_connected());
}

TEST_F(TtsAudioOutputStreamFactoryTest,
       DisconnectsAudioServiceWhenClientDisconnects) {
  mojo::Remote<media::mojom::AudioStreamFactory> factory(
      TtsAudioOutputStreamFactory::Create(mock_audio_service_.MakeRemote()));

  // Reset the client remote to simulate the TTS utility service dropping the
  // pipe.
  factory.reset();

  // The wrapper should close its audio_service_factory_ remote, triggering
  // disconnect on the underlying audio service.
  mock_audio_service_.WaitForDisconnect();
}

}  // namespace chromeos::tts
