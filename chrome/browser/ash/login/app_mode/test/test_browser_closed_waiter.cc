// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TestBrowserClosedWaiter::TestBrowserClosedWaiter(Browser* browser)
    : browser_{browser} {
  BrowserList::AddObserver(this);
}

TestBrowserClosedWaiter::~TestBrowserClosedWaiter() {
  BrowserList::RemoveObserver(this);
}

void TestBrowserClosedWaiter::WaitUntilClosed() {
  ASSERT_TRUE(future_.Wait()) << "Browser was not closed";
}

void TestBrowserClosedWaiter::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser) {
    future_.SetValue(true);
  }
}

}  // namespace ash
