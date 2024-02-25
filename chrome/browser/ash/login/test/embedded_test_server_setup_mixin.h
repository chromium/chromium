// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace ash {

// An InProcessBrowserTestMixin that sets up an embedded test server
// to serve data from test data directory.
class EmbeddedTestServerSetupMixin : public InProcessBrowserTestMixin {
 public:
  EmbeddedTestServerSetupMixin(InProcessBrowserTestMixinHost* host,
                               net::EmbeddedTestServer* server);

  EmbeddedTestServerSetupMixin(const EmbeddedTestServerSetupMixin&) = delete;
  EmbeddedTestServerSetupMixin& operator=(const EmbeddedTestServerSetupMixin&) =
      delete;

  ~EmbeddedTestServerSetupMixin() override;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void TearDownOnMainThread() override;
  void SetUpOnMainThread() override;

 private:
  // Path to directory served by embedded test server.
  base::FilePath test_data_dir_;

  // Embedded test server owned by test that uses this mixin.
  raw_ptr<net::EmbeddedTestServer> embedded_test_server_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_TEST_SERVER_SETUP_MIXIN_H_
