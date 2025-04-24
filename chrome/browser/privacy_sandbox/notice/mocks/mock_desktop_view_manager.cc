// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"

using ::testing::Return;

namespace privacy_sandbox {

MockDesktopViewManager::MockDesktopViewManager() {
  test_navigation_handler_ =
      std::make_unique<NavigationHandler>(base::BindRepeating(
          &MockDesktopViewManager::HandleChromeOwnedPageNavigation,
          base::Unretained(this)));
  ON_CALL(*this, GetNavigationHandler())
      .WillByDefault(Return(test_navigation_handler_.get()));
}

MockDesktopViewManager::~MockDesktopViewManager() = default;

}  // namespace privacy_sandbox
