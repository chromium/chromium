// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

namespace chromeos {

EmbeddedTestServerSetupMixin::EmbeddedTestServerSetupMixin(
    InProcessBrowserTestMixinHost* host,
    net::EmbeddedTestServer* server)
    : InProcessBrowserTestMixin(host), embedded_test_server_(server) {}

EmbeddedTestServerSetupMixin::~EmbeddedTestServerSetupMixin() = default;

void EmbeddedTestServerSetupMixin::SetUp() {
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  embedded_test_server_->ServeFilesFromDirectory(test_data_dir_);
  // Don't spin up the IO thread yet since no threads are allowed while
  // spawning sandbox host process. See crbug.com/322732.
  CHECK(embedded_test_server_->InitializeAndListen());
}

void EmbeddedTestServerSetupMixin::SetUpOnMainThread() {
  embedded_test_server_->StartAcceptingConnections();
}

void EmbeddedTestServerSetupMixin::TearDownOnMainThread() {
  // Embedded test server should always be shutdown after any https forwarders.
  CHECK(embedded_test_server_->ShutdownAndWaitUntilComplete());
}

}  // namespace chromeos
