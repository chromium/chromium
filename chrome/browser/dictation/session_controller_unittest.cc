// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dictation {

namespace {

class DictationSessionControllerTest : public testing::Test {
 public:
  DictationSessionControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
    controller_ = std::make_unique<SessionController>(mock_delegate_);
  }
  ~DictationSessionControllerTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSessionControllerDelegate> mock_delegate_;
  std::unique_ptr<SessionController> controller_;
};

TEST_F(DictationSessionControllerTest, StartsInactive) {
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that starting and stopping a stream moves the controller into the
// appropriate state.
TEST_F(DictationSessionControllerTest, StreamAffectsState) {
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
  EXPECT_NE(controller_->attached_stream_provider(), nullptr);

  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that starting a stream initializes the stream provider and binds it to
// the given target.
TEST_F(DictationSessionControllerTest, StartStreamInitializesStreamProvider) {
  std::string selected_text = "test_selection";
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  // Starting a stream should create a stream provider and bind it to the given
  // target.
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  EXPECT_CALL(*stream_provider_ptr, BindToTargetAndConnect(_))
      .WillOnce([selected_text](std::unique_ptr<Target> passed_target) {
        EXPECT_EQ(passed_target->GetSelectedText(), selected_text);
      });
  controller_->StartDictationStream(EmptyTargetId(), selected_text);
}

// Test that ending a stream notifies the stream provider to stop.
TEST_F(DictationSessionControllerTest, EndStream) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");

  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
}

// Test that calling EndDictationStream while the controller is in the
// kStreamInitializing state transitions the controller to kFinalizing.
TEST_F(DictationSessionControllerTest, EndStreamDuringInitialization) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  ASSERT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that registering a callback receives state updates when the controller
// transitions states.
TEST_F(DictationSessionControllerTest, StateChangedCallback) {
  std::vector<SessionState> states;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(base::BindLambdaForTesting(
          [&](SessionState state) { states.push_back(state); }));

  controller_->StartDictationStream(EmptyTargetId(), "");
  controller_->EndDictationStream();

  EXPECT_THAT(states, testing::ElementsAre(SessionState::kStreamInitializing,
                                           SessionState::kFinalizing));
}

// Test that propagating state changes from the stream provider updates the
// controller's state accordingly.
TEST_F(DictationSessionControllerTest, StreamProviderStatePropagates) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // Transition to complete.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));

  // Transitions to inactive are asynchronous.
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(
          base::BindLambdaForTesting([&](SessionState state) {
            if (state == SessionState::kInactive) {
              run_loop.Quit();
            }
          }));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  run_loop.Run();

  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that propagating state changes for a failure
TEST_F(DictationSessionControllerTest, StreamProviderStatePropagatesFailure) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // Transition to failure.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));

  // Transitions to inactive are asynchronous.
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(
          base::BindLambdaForTesting([&](SessionState state) {
            if (state == SessionState::kInactive) {
              run_loop.Quit();
            }
          }));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  run_loop.Run();

  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that stopping an active transcribing stream enters kFinalizing, and
// transitioning that stream to kComplete transitions the controller to
// kInactive.
TEST_F(DictationSessionControllerTest, FinalizeStreamToComplete) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // End the stream. It should transition to kFinalizing.
  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the finalizing stream to complete.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that stopping an active transcribing stream enters kFinalizing, and
// transitioning that stream to kFailed transitions the controller to
// kInactive.
TEST_F(DictationSessionControllerTest, FinalizeStreamToFailed) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // End the stream. It should transition to kFinalizing.
  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the finalizing stream to failed.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that once a stream is finalizing, a new stream can be started, which
// transitions the controller to kStreamInitializing.
TEST_F(DictationSessionControllerTest, StartNewStreamWhileFinalizing) {
  auto mock_stream_provider_1 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_1_ptr = mock_stream_provider_1.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_1)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_1_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_1_ptr, StreamProvider::StreamState::kInitializing);

  // End the first stream. It should transition to kFinalizing.
  EXPECT_CALL(*stream_provider_1_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);

  // Start a second stream. This should be allowed and transition the
  // controller back to kStreamInitializing.
  auto mock_stream_provider_2 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_2_ptr = mock_stream_provider_2.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_2)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
  EXPECT_EQ(controller_->attached_stream_provider(), stream_provider_2_ptr);

  // Transition the first stream to complete. This should have no effect as
  // there's an attached stream.
  EXPECT_CALL(*stream_provider_1_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_1_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition the second stream to kFailed now. This should now move the
  // controller to kInactive since there's no finalizing streams.
  EXPECT_CALL(*stream_provider_2_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_2_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that if multiple streams are finalizing at the same time, the
// controller state remains kFinalizing until all streams have completed.
TEST_F(DictationSessionControllerTest, MultipleFinalizingStreams) {
  // Start and end the first stream.
  auto mock_stream_provider_1 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_1_ptr = mock_stream_provider_1.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_1)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_CALL(*stream_provider_1_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Start and end the second stream.
  auto mock_stream_provider_2 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_2_ptr = mock_stream_provider_2.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_2)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_CALL(*stream_provider_2_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the first stream to complete. The controller should remain
  // in kFinalizing.
  EXPECT_CALL(*stream_provider_1_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_1_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the second stream to complete. The controller should now
  // transition to kInactive.
  EXPECT_CALL(*stream_provider_2_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_2_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that a stream provider that has been moved to the finalizing set
// cannot transition the controller state back to active states (kInitializing
// or kTranscribing) and those updates are ignored.
TEST_F(DictationSessionControllerTest, FinalizingStreamStateChangesIgnored) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTargetId(), "");
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // End the stream. It should transition to kFinalizing.
  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the finalizing stream to kTranscribing. This should be ignored.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Transition the finalizing stream to kInitializing. This should be ignored.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kInitializing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
}

// Test that a stream provider that is completely untracked (neither the
// attached provider nor in the finalizing set) sending state updates has
// absolutely no effect on the controller's state.
TEST_F(DictationSessionControllerTest, UntrackedStreamStateChangesIgnored) {
  testing::NiceMock<MockStreamProvider> untracked_stream_provider;
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);

  // Untracked provider transitions to kInitializing.
  ON_CALL(untracked_stream_provider, GetState())
      .WillByDefault(Return(StreamProvider::StreamState::kInitializing));
  controller_->DidUpdateStreamProviderState(
      untracked_stream_provider, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);

  // Untracked provider transitions to kTranscribing.
  ON_CALL(untracked_stream_provider, GetState())
      .WillByDefault(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      untracked_stream_provider, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);

  // Untracked provider transitions to kComplete.
  ON_CALL(untracked_stream_provider, GetState())
      .WillByDefault(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      untracked_stream_provider, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);

  // Untracked provider transitions to kFailed.
  ON_CALL(untracked_stream_provider, GetState())
      .WillByDefault(Return(StreamProvider::StreamState::kFailed));
  controller_->DidUpdateStreamProviderState(
      untracked_stream_provider, StreamProvider::StreamState::kTranscribing);
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

}  // namespace

}  // namespace dictation
