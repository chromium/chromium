// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/presentation_time_callback_buffer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<cc::PresentationTimeCallbackBuffer::Callback> GenerateCallbacks(
    int num_callbacks) {
  std::vector<cc::PresentationTimeCallbackBuffer::Callback> result;

  while (num_callbacks-- > 0) {
    // `PresentationTimeCallbackBuffer` isn't supposed to invoke any callbacks.
    // We can check for that by passing callbacks which cause test failure.
    result.push_back(base::BindOnce([](const gfx::PresentationFeedback&) {
      FAIL() << "Callbacks should not be directly invoked by "
                "PresentationTimeCallbackBuffer";
    }));
  }

  return result;
}

std::vector<cc::PresentationTimeCallbackBuffer::SuccessfulCallback>
GenerateSuccessfulCallbacks(int num_callbacks) {
  std::vector<cc::PresentationTimeCallbackBuffer::SuccessfulCallback> result;

  while (num_callbacks-- > 0) {
    // `PresentationTimeCallbackBuffer` isn't supposed to invoke any callbacks.
    // We can check for that by passing callbacks which cause test failure.
    result.push_back(base::BindOnce([](base::TimeTicks presentation_timestamp) {
      FAIL() << "Callbacks should not be directly invoked by "
                "PresentationTimeCallbackBuffer";
    }));
  }

  return result;
}

std::vector<cc::PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
GenerateSuccessfulCallbackWithDetails(int num_callbacks) {
  std::vector<cc::PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
      result;

  while (num_callbacks-- > 0) {
    // `PresentationTimeCallbackBuffer` isn't supposed to invoke any callbacks.
    // We can check for that by passing callbacks which cause test failure.
    result.push_back(base::BindOnce([](const viz::FrameTimingDetails& details) {
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

  auto result =
      buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);

  EXPECT_TRUE(result.main_callbacks.empty());
  EXPECT_TRUE(result.main_successful_callbacks.empty());
  EXPECT_TRUE(result.compositor_successful_callbacks.empty());
}

TEST(PresentationTimeCallbackBufferTest, TestMainThreadCallbackOnSuccess) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadCallbacks(kFrameToken2, GenerateCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is returned on a successful presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_EQ(result.main_callbacks.size(), 1ull);
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestMainThreadCallbackOnFailure) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadCallbacks(kFrameToken2, GenerateCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is returned on a failed presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/true);
    EXPECT_EQ(result.main_callbacks.size(), 1ull);
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestMainThreadSuccessfulCallback) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbackWithDetails(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is not returned on a failed presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/true);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is returned on a successful presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_EQ(result.main_successful_callbacks.size(), 1ull);
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest,
     TestCompositorThreadSuccessfulCallback) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterCompositorThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is not returned on a failed presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/true);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure the callback is returned on a successful presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_EQ(result.compositor_successful_callbacks.size(), 1ull);
  }

  // Make sure that the buffer has removed the registration since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestMixedCallbacksOnSuccess) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadCallbacks(kFrameToken2, GenerateCallbacks(1));
  buffer.RegisterMainThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbackWithDetails(1));
  buffer.RegisterCompositorThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure all callbacks are returned on a successful presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_EQ(result.main_callbacks.size(), 1ull);
    EXPECT_EQ(result.main_successful_callbacks.size(), 1ull);
    EXPECT_EQ(result.compositor_successful_callbacks.size(), 1ull);
  }

  // Make sure that the buffer has removed the registrations since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestMixedCallbacksOnFailure) {
  PresentationTimeCallbackBuffer buffer;

  buffer.RegisterMainThreadCallbacks(kFrameToken2, GenerateCallbacks(1));
  buffer.RegisterMainThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbackWithDetails(1));
  buffer.RegisterCompositorThreadSuccessfulCallbacks(
      kFrameToken2, GenerateSuccessfulCallbacks(1));

  // Make sure that popping early frame tokens doesn't return irrelevant
  // entries.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken1, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure only feedback callbacks are returned on a failed presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/true);
    EXPECT_EQ(result.main_callbacks.size(), 1ull);
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }

  // Make sure time callbacks are returned on a successful presentation.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_EQ(result.main_successful_callbacks.size(), 1ull);
    EXPECT_EQ(result.compositor_successful_callbacks.size(), 1ull);
  }

  // Make sure that the buffer has removed the registrations since the "pop".
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken2, /*presentation_failed=*/false);
    EXPECT_TRUE(result.main_callbacks.empty());
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

TEST(PresentationTimeCallbackBufferTest, TestCallbackBatching) {
  PresentationTimeCallbackBuffer buffer;

  // Register one callback for frame1, two for frame2 and two for frame4.
  buffer.RegisterMainThreadCallbacks(kFrameToken1, GenerateCallbacks(1));
  buffer.RegisterMainThreadCallbacks(kFrameToken2, GenerateCallbacks(2));
  buffer.RegisterMainThreadCallbacks(kFrameToken4, GenerateCallbacks(2));

  // Pop callbacks up to and including frame3. Should be three in total; one
  // from frame1 and two from frame2.
  {
    auto result =
        buffer.PopPendingCallbacks(kFrameToken3, /*presentation_failed=*/false);
    EXPECT_EQ(result.main_callbacks.size(), 3ull);
    EXPECT_TRUE(result.main_successful_callbacks.empty());
    EXPECT_TRUE(result.compositor_successful_callbacks.empty());
  }
}

}  // namespace cc
