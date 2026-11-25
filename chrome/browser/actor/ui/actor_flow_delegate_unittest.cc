// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_flow_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {

TEST(ActorFlowDelegateTest, HidesBrowserUiByDefault) {
  ActorFlowDelegate flow_delegate;
  EXPECT_FALSE(flow_delegate.ShouldShowBrowserUi());
}

TEST(ActorFlowDelegateTest, BrowserUiDelegateShowsBrowserUi) {
  BrowserUiActorFlowDelegate flow_delegate;
  EXPECT_TRUE(flow_delegate.ShouldShowBrowserUi());
}

}  // namespace actor::ui
