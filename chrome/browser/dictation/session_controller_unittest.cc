// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include "base/test/scoped_feature_list.h"
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
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSessionControllerDelegate> mock_delegate_;
  std::unique_ptr<SessionController> controller_;
};

TEST_F(DictationSessionControllerTest, StartsInactive) {
  EXPECT_EQ(controller_->state(), SessionController::State::kInactive);
}

// Test that starting and stopping a stream moves the controller into the
// appropriate state.
TEST_F(DictationSessionControllerTest, StreamAffectsState) {
  MockTarget target;
  controller_->StartDictationStream(target);
  EXPECT_EQ(controller_->state(),
            SessionController::State::kStreamInitializing);
  EXPECT_NE(controller_->attached_stream_provider(), nullptr);

  controller_->EndDictationStream();
  EXPECT_EQ(controller_->state(), SessionController::State::kInactive);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that starting a stream initializes the stream provider and binds it to
// the given target.
TEST_F(DictationSessionControllerTest, StartStreamInitializesStreamProvider) {
  MockTarget target;
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  // Starting a stream should create a stream provider and bind it to the given
  // target.
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  EXPECT_CALL(*stream_provider_ptr, BindToTarget(testing::Ref(target)));
  controller_->StartDictationStream(target);
}

// Test that ending a stream notifies the stream provider to stop.
TEST_F(DictationSessionControllerTest, EndStream) {
  MockTarget target;
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(target);

  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
}

}  // namespace

}  // namespace dictation
