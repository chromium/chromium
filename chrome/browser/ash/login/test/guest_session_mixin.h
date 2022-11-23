// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_GUEST_SESSION_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_GUEST_SESSION_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace base {
class CommandLine;
}

namespace ash {

// A mixin that sets up test command line for guest user session.
// Use this with tests for in-session behavior for guest user.
class GuestSessionMixin : public InProcessBrowserTestMixin {
 public:
  explicit GuestSessionMixin(InProcessBrowserTestMixinHost* mixin_host);

  GuestSessionMixin(const GuestSessionMixin&) = delete;
  GuestSessionMixin& operator=(const GuestSessionMixin&) = delete;

  ~GuestSessionMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_GUEST_SESSION_MIXIN_H_
