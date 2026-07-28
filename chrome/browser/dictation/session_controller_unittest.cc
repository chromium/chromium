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
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/editable_level.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_dom_node_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"

using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

namespace dictation {

namespace {

class DictationSessionControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  DictationSessionControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
    controller_ = std::make_unique<SessionController>(mock_delegate_);
  }
  ~DictationSessionControllerTest() override = default;

  content::GlobalDOMNodeId MockTargetInMainFrame(int dom_node_id) {
    return content::GlobalDOMNodeId{main_rfh()->GetWeakDocumentPtr(),
                                    blink::DOMNodeIdType(dom_node_id)};
  }

  void WaitForPostedTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
  EXPECT_NE(controller_->attached_stream_provider(), nullptr);

  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that starting a stream initializes the stream provider and binds it to
// the given target.
TEST_F(DictationSessionControllerTest, StartStreamInitializesStreamProvider) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  // Starting a stream should create a stream provider and bind it to the given
  // target.
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  EXPECT_CALL(*stream_provider_ptr, BindToTargetAndConnect(_)).Times(1);
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
}

// Test that ending a stream notifies the stream provider to stop.
TEST_F(DictationSessionControllerTest, EndStream) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);

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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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

  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
  EXPECT_CALL(*stream_provider_1_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Start and end the second stream.
  auto mock_stream_provider_2 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_2_ptr = mock_stream_provider_2.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_2)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
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

TEST_F(DictationSessionControllerTest, DoNotEndStreamOnNonUserFocusChange) {
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  for (auto focus_type :
       {blink::mojom::FocusType::kNone, blink::mojom::FocusType::kScript}) {
    content::FocusedNodeDetails details;
    details.focus_type = focus_type;
    details.editable_level = content::EditableLevel::kPlaintextEditable;
    details.global_dom_node_id = MockTargetInMainFrame(1);

    controller_->OnFocusChangedInPage(details);
    // Stream should remain active.
    EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
    EXPECT_NE(controller_->attached_stream_provider(), nullptr);
  }
}

TEST_F(DictationSessionControllerTest, EndStreamOnFocusNonEditableNode) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);

  EXPECT_CALL(*stream_provider_ptr, Stop());
  content::FocusedNodeDetails details;
  details.focus_type = blink::mojom::FocusType::kMouse;
  details.editable_level = content::EditableLevel::kNotEditable;
  details.global_dom_node_id = MockTargetInMainFrame(1);

  controller_->OnFocusChangedInPage(details);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

TEST_F(DictationSessionControllerTest, StartNewStreamOnFocusOtherEditableNode) {
  Target target_1(EmptyTarget());
  auto mock_stream_provider_1 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_1_ptr = mock_stream_provider_1.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_1)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);

  EXPECT_CALL(*stream_provider_1_ptr, Stop());

  EXPECT_CALL(*stream_provider_1_ptr, GetTarget())
      .WillRepeatedly(Return(&target_1));

  auto mock_stream_provider_2 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_2_ptr = mock_stream_provider_2.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_2)));

  EXPECT_CALL(*stream_provider_2_ptr, BindToTargetAndConnect(Pointee(Property(
                                          &Target::richly_editable, true))));

  content::FocusedNodeDetails details;
  details.focus_type = blink::mojom::FocusType::kMouse;
  details.editable_level = content::EditableLevel::kRichlyEditable;
  details.global_dom_node_id = MockTargetInMainFrame(1);

  controller_->OnFocusChangedInPage(details);
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
  EXPECT_EQ(controller_->attached_stream_provider(), stream_provider_2_ptr);
}

TEST_F(DictationSessionControllerTest,
       DoNotStartNewStreamOnFocusElementWithExistingStream) {
  content::GlobalDOMNodeId target_id_1 = MockTargetInMainFrame(1);
  Target target_1(TargetDetails{target_id_1});

  auto mock_stream_provider_1 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_1_ptr = mock_stream_provider_1.get();

  EXPECT_CALL(*stream_provider_1_ptr, GetTarget())
      .WillRepeatedly(Return(&target_1));

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_1)));
  controller_->StartDictationStream(TargetDetails{target_id_1},
                                    DictationStreamStartTrigger::kSessionStart);

  EXPECT_CALL(*stream_provider_1_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // While a stream is finalizing for an element, change focus to the same
  // element. This should not start a new stream.
  content::FocusedNodeDetails details;
  details.focus_type = blink::mojom::FocusType::kMouse;
  details.editable_level = content::EditableLevel::kPlaintextEditable;
  details.global_dom_node_id = MockTargetInMainFrame(1);

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_)).Times(0);
  controller_->OnFocusChangedInPage(details);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

TEST_F(DictationSessionControllerTest,
       DoNotStartNewStreamOnFocusElementDuringShutdown) {
  content::GlobalDOMNodeId target_id_1 = MockTargetInMainFrame(1);
  Target target_1(TargetDetails{target_id_1});

  auto mock_stream_provider_1 =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_1_ptr = mock_stream_provider_1.get();

  EXPECT_CALL(*stream_provider_1_ptr, GetTarget())
      .WillRepeatedly(Return(&target_1));

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider_1)));
  controller_->StartDictationStream(TargetDetails{target_id_1},
                                    DictationStreamStartTrigger::kSessionStart);

  EXPECT_CALL(*stream_provider_1_ptr, Stop());
  controller_->FinalizeAndShutdown();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // The session is pending shutdown with a finalizing stream. We cannot create
  // additional streams in this state. Simulate a focus change, which should not
  // start a new stream.
  content::FocusedNodeDetails details;
  details.focus_type = blink::mojom::FocusType::kMouse;
  details.editable_level = content::EditableLevel::kPlaintextEditable;
  details.global_dom_node_id = MockTargetInMainFrame(2);

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_)).Times(0);
  controller_->OnFocusChangedInPage(details);
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that transitioning an active stream provider to a failure state
// calls OnError on the UI.
TEST_F(DictationSessionControllerTest, ActiveStreamFailureOnErrorCalled) {
  auto mock_ui = std::make_unique<testing::NiceMock<MockSessionUi>>();
  MockSessionUi* ui_ptr = mock_ui.get();
  EXPECT_CALL(mock_delegate_, CreateUi(_)).WillOnce(Return(std::move(mock_ui)));
  controller_->ResetUi();

  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);

  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));
  EXPECT_CALL(*ui_ptr, OnError(SessionUi::StreamType::kAttached));

  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
}

// Test that transitioning a completed stream provider to a failure state
// does not call OnError on the UI.
TEST_F(DictationSessionControllerTest, CompletedStreamFailureOnErrorNotCalled) {
  auto mock_ui = std::make_unique<testing::NiceMock<MockSessionUi>>();
  MockSessionUi* ui_ptr = mock_ui.get();
  EXPECT_CALL(mock_delegate_, CreateUi(_)).WillOnce(Return(std::move(mock_ui)));
  controller_->ResetUi();

  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);

  // First transition to complete.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);

  // Now transition to failed. OnError should not be called.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));
  EXPECT_CALL(*ui_ptr, OnError(_)).Times(0);

  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kComplete);
}

// Test that UpdateAudioLevel propagates to the UI.
TEST_F(DictationSessionControllerTest, UpdateAudioLevelPropagatesToUi) {
  auto mock_ui = std::make_unique<testing::NiceMock<MockSessionUi>>();
  MockSessionUi* ui_ptr = mock_ui.get();
  EXPECT_CALL(mock_delegate_, CreateUi(_)).WillOnce(Return(std::move(mock_ui)));
  controller_->ResetUi();

  EXPECT_CALL(*ui_ptr, UpdateAudioLevel(0.8f));
  controller_->UpdateAudioLevel(0.8f);
}

TEST_F(DictationSessionControllerTest,
       FinalizeAndShutdownSessionEndedAfterFinalization) {
  controller_->ResetUi();

  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
  auto* stream_provider = controller_->attached_stream_provider();
  ASSERT_NE(stream_provider, nullptr);

  EXPECT_CALL(static_cast<MockStreamProvider&>(*stream_provider), GetState())
      .WillRepeatedly(
          testing::Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  controller_->FinalizeAndShutdown();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // Complete stream finalization and verify EndSession is called on delegate.
  EXPECT_CALL(static_cast<MockStreamProvider&>(*stream_provider), GetState())
      .WillRepeatedly(testing::Return(StreamProvider::StreamState::kComplete));
  EXPECT_CALL(mock_delegate_, EndSession()).WillOnce([this]() {
    controller_.reset();
  });

  controller_->DidUpdateStreamProviderState(
      *stream_provider, StreamProvider::StreamState::kTranscribing);
  WaitForPostedTasks();

  EXPECT_EQ(controller_, nullptr);
}

TEST_F(DictationSessionControllerTest,
       FinalizeAndShutdownNewStreamAbortsEndSession) {
  controller_->ResetUi();

  controller_->StartDictationStream(EmptyTarget(),
                                    DictationStreamStartTrigger::kSessionStart);
  auto* stream_provider1 = controller_->attached_stream_provider();
  ASSERT_NE(stream_provider1, nullptr);

  EXPECT_CALL(static_cast<MockStreamProvider&>(*stream_provider1), GetState())
      .WillRepeatedly(
          testing::Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider1, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // Call FinalizeAndShutdown, placing stream 1 in finalization.
  controller_->FinalizeAndShutdown();
  EXPECT_EQ(controller_->GetState(), SessionState::kFinalizing);

  // While stream 1 is finalizing, start a new stream (stream 2).
  controller_->StartDictationStream(
      EmptyTarget(), DictationStreamStartTrigger::kContextMenuExistingSession);
  auto* stream_provider2 = controller_->attached_stream_provider();
  ASSERT_NE(stream_provider2, nullptr);
  EXPECT_NE(stream_provider1, stream_provider2);
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Complete finalization on stream 1 and verify EndSession is NOT called.
  EXPECT_CALL(static_cast<MockStreamProvider&>(*stream_provider1), GetState())
      .WillRepeatedly(testing::Return(StreamProvider::StreamState::kComplete));
  EXPECT_CALL(mock_delegate_, EndSession()).Times(0);

  controller_->DidUpdateStreamProviderState(
      *stream_provider1, StreamProvider::StreamState::kTranscribing);
  WaitForPostedTasks();

  EXPECT_NE(controller_, nullptr);
  EXPECT_EQ(controller_->attached_stream_provider(), stream_provider2);
}

}  // namespace

}  // namespace dictation
