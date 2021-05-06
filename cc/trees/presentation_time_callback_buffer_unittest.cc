// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/presentation_time_callback_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<cc::PresentationTimeCallbackBuffer::CallbackType> GenerateCallbacks(
    int num_callbacks) {
  std::vector<cc::PresentationTimeCallbackBuffer::CallbackType> result;

  while (num_callbacks-- > 0) {
    // PresentationTimeCallbackBuffer isn't supposed to invoke any callbacks.
    // We can check for that by passing callbacks which cause test failure.
    result.push_back(base::BindOnce([](const gfx::PresentationFeedback&) {
      FAIL() << "Callbacks should not be directly invoked by "
                "PresentationTimeCallbackBuffer";
    }));
  }

  return result;
}

constexpr uint32_t kFrameToken1 = 234;
constexpr uint32_t kFrameToken2 = 345;
constexpr uint32_t kFrameToken3 = 456;
constexpr uint32_t kFrameToken4 = 567;

}  // namespace

namespace cc {

TEST(PresentationTimeCallbackBufferTest, TestNoCallbacks) {
  PresentationTimeCallbackBuffer buffer;

  auto result = buffer.PopPendingCallbacks(kFrameToken1);

  EXPECT_TRUE(result.main_thread_callbacks.empty());
  EXPECT_TRUE(result.compositor_thread_callbacks.empty());
}

TEST(PresentationTimeCallbackBufferTest, TestOneMainThreadCallback) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadPresentationCallbacks(kFrameToken2,
                                                 GenerateCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken1);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }

  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_EQ(result.main_thread_callbacks.size(), 1ull);
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestOneCompositorThreadCallback) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterCompositorPresentationCallbacks(kFrameToken2,
                                                 GenerateCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken1);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }

  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_EQ(result.compositor_thread_callbacks.size(), 1ull);
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestMixedCallbacks) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadPresentationCallbacks(kFrameToken2,
                                                 GenerateCallbacks(1));
  buffer.RegisterCompositorPresentationCallbacks(kFrameToken2,
                                                 GenerateCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken1);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }

  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_EQ(result.main_thread_callbacks.size(), 1ull);
    EXPECT_EQ(result.compositor_thread_callbacks.size(), 1ull);
  }

  // Make sure that the buffer has removed the registrations since the "pop".
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken2);
    EXPECT_TRUE(result.main_thread_callbacks.empty());
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestCallbackBatching) {
  PresentationTimeCallbackBuffer buffer;

  // Register one callback for frame1, two for frame2 and two for frame4.
  buffer.RegisterMainThreadPresentationCallbacks(kFrameToken1,
                                                 GenerateCallbacks(1));
  buffer.RegisterMainThreadPresentationCallbacks(kFrameToken2,
                                                 GenerateCallbacks(2));
  buffer.RegisterMainThreadPresentationCallbacks(kFrameToken4,
                                                 GenerateCallbacks(2));

  // Pop callbacks up to and including frame3. Should be three in total; one
  // from frame1 and two from frame2.
  {
    auto result = buffer.PopPendingCallbacks(kFrameToken3);
    EXPECT_EQ(result.main_thread_callbacks.size(), 3ull);
    EXPECT_TRUE(result.compositor_thread_callbacks.empty());
  }
}

}  // namespace cc
