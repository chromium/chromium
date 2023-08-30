// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_BROWSER_CLOSED_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_BROWSER_CLOSED_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace ash {

class TestBrowserClosedWaiter : public BrowserListObserver {
 public:
  explicit TestBrowserClosedWaiter(Browser* browser);

  ~TestBrowserClosedWaiter() override;

  void WaitUntilClosed();

 private:
  void OnBrowserRemoved(Browser* browser) override;

  raw_ptr<Browser, DanglingUntriaged | ExperimentalAsh> browser_ = nullptr;
  base::test::TestFuture<bool> future_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_BROWSER_CLOSED_WAITER_H_
