// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_EMBEDDED_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_EMBEDDED_TEST_SERVER_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace chromeos {

// An InProcessBrowserTestMixin that sets up an embedded test server
// to serve data from test data directory.
class EmbeddedTestServerSetupMixin : public InProcessBrowserTestMixin {
 public:
  EmbeddedTestServerSetupMixin(InProcessBrowserTestMixinHost* host,
                               net::EmbeddedTestServer* server);
  ~EmbeddedTestServerSetupMixin() override;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void TearDownOnMainThread() override;
  void SetUpOnMainThread() override;

 private:
  // Path to directory served by embedded test server.
  base::FilePath test_data_dir_;

  // Embedded test server owned by test that uses this mixin.
  net::EmbeddedTestServer* embedded_test_server_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServerSetupMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_EMBEDDED_TEST_SERVER_MIXIN_H_
