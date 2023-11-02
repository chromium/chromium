// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FrameNavigationStateTest : public ChromeRenderViewHostTestHarness {};

// Test that no events can be sent for a frame after an error occurred, but
// before a new navigation happened in this frame.
TEST_F(FrameNavigationStateTest, ErrorState) {
  const GURL url("http://www.google.com/");
  auto* navigation_state =
      FrameNavigationState::GetOrCreateForCurrentDocument(main_rfh());

  navigation_state->StartTrackingDocumentLoad(url, false, false, false);
  EXPECT_TRUE(navigation_state->CanSendEvents());
  EXPECT_FALSE(navigation_state->GetErrorOccurredInFrame());

  // After an error occurred, no further events should be sent.
  navigation_state->SetErrorOccurredInFrame();
  EXPECT_FALSE(navigation_state->CanSendEvents());
  EXPECT_TRUE(navigation_state->GetErrorOccurredInFrame());

  // Navigations to a network error page should be ignored.
  navigation_state->StartTrackingDocumentLoad(GURL(), false, false, true);
  EXPECT_FALSE(navigation_state->CanSendEvents());
  EXPECT_TRUE(navigation_state->GetErrorOccurredInFrame());

  // However, when the frame navigates again, it should send events again.
  navigation_state->StartTrackingDocumentLoad(url, false, false, false);
  EXPECT_TRUE(navigation_state->CanSendEvents());
  EXPECT_FALSE(navigation_state->GetErrorOccurredInFrame());
}

// Tests that no events are send for a not web-safe scheme.
TEST_F(FrameNavigationStateTest, WebSafeScheme) {
  const GURL url("unsafe://www.google.com/");
  auto* navigation_state =
      FrameNavigationState::GetOrCreateForCurrentDocument(main_rfh());

  navigation_state->StartTrackingDocumentLoad(url, false, false, false);
  EXPECT_FALSE(navigation_state->CanSendEvents());
}

// Test for <iframe srcdoc=""> frames.
TEST_F(FrameNavigationStateTest, SrcDoc) {
  const GURL srcdoc("about:srcdoc");
  auto* navigation_state =
      FrameNavigationState::GetOrCreateForCurrentDocument(main_rfh());

  navigation_state->StartTrackingDocumentLoad(srcdoc, false, false, false);
  EXPECT_TRUE(navigation_state->CanSendEvents());
  EXPECT_EQ(srcdoc, navigation_state->GetUrl());
  EXPECT_TRUE(FrameNavigationState::IsValidUrl(srcdoc));
}

}  // namespace extensions
