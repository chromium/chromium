// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_audio_broker.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/readaloud/fake_audio_stream_factory.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {
namespace {

class ReadAloudAudioBrokerTest : public testing::Test {
 public:
  ReadAloudAudioBrokerTest()
      : broker_(base::BindRepeating(&FakeAudioStreamFactory::Bind,
                                    base::Unretained(&fake_factory_))) {}

  void SetUp() override {
    test_params_ = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Mono(), readaloud::kAudioSampleRate,
        readaloud::kAudioFramesPerBuffer);
    test_group_id_ = base::UnguessableToken::Create();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeAudioStreamFactory fake_factory_;
  ReadAloudAudioBroker broker_;
  media::AudioParameters test_params_;
  base::UnguessableToken test_group_id_;
};

TEST_F(ReadAloudAudioBrokerTest, CreateOutputStreamSuccess) {
  fake_factory_.set_auto_respond(/*auto_respond=*/true,
                                 /*should_succeed=*/true);

  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future.GetCallback());

  auto [received_stream, received_pipe] = future.Take();

  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 1);
  EXPECT_EQ(fake_factory_.last_device_id(),
            media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_EQ(fake_factory_.last_group_id(), test_group_id_);
  EXPECT_EQ(fake_factory_.last_params().sample_rate(),
            test_params_.sample_rate());
  EXPECT_EQ(fake_factory_.last_params().frames_per_buffer(),
            test_params_.frames_per_buffer());

  EXPECT_TRUE(received_stream.is_valid());
  ASSERT_TRUE(received_pipe);
  EXPECT_TRUE(received_pipe->shared_memory.IsValid());
  EXPECT_TRUE(received_pipe->socket.is_valid());
}

TEST_F(ReadAloudAudioBrokerTest, CreateOutputStreamFailureReturnsNullPipe) {
  fake_factory_.set_auto_respond(/*auto_respond=*/true,
                                 /*should_succeed=*/false);

  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future.GetCallback());

  auto [received_stream, received_pipe] = future.Take();

  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 1);
  EXPECT_TRUE(received_stream.is_valid());
  EXPECT_FALSE(received_pipe);
}

TEST_F(ReadAloudAudioBrokerTest,
       FactoryDisconnectInFlightInvokesCallbackWithNullPipe) {
  fake_factory_.set_auto_respond(/*auto_respond=*/false);

  base::test::TestFuture<void> factory_future;
  fake_factory_.set_create_output_stream_callback(factory_future.GetCallback());

  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future.GetCallback());

  EXPECT_TRUE(factory_future.Wait());
  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 1);

  // Simulate factory disconnect while creation is in-flight.
  // WrapCallbackWithDefaultInvokeIfNotRun ensures the callback is invoked with
  // null data pipe.
  fake_factory_.Disconnect();

  auto [received_stream, received_pipe] = future.Take();
  EXPECT_TRUE(received_stream.is_valid());
  EXPECT_FALSE(received_pipe);
}

TEST_F(ReadAloudAudioBrokerTest, ResetCancelsPendingCallback) {
  fake_factory_.set_auto_respond(/*auto_respond=*/false);

  base::test::TestFuture<void> factory_future;
  fake_factory_.set_create_output_stream_callback(factory_future.GetCallback());

  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future.GetCallback());

  EXPECT_TRUE(factory_future.Wait());
  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 1);

  // Reset cancels pending callbacks in ReadAloudAudioBroker.
  broker_.Reset();

  fake_factory_.RespondWithLastCallback(/*succeed=*/true);

  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(ReadAloudAudioBrokerTest, FactoryDisconnectRebindsOnNextRequest) {
  fake_factory_.set_auto_respond(/*auto_respond=*/true,
                                 /*should_succeed=*/true);

  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future1;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future1.GetCallback());
  EXPECT_TRUE(future1.Wait());
  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 1);

  // Simulate factory disconnect and flush disconnection on current sequence.
  fake_factory_.Disconnect();
  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();

  // Next request should re-bind and succeed.
  base::test::TestFuture<mojo::PendingRemote<media::mojom::AudioOutputStream>,
                         media::mojom::ReadWriteAudioDataPipePtr>
      future2;
  broker_.CreateOutputStream(test_group_id_, test_params_,
                             future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
  EXPECT_EQ(fake_factory_.create_output_stream_called_count(), 2);
}

}  // namespace
}  // namespace readaloud
