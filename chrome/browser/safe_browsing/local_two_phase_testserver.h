// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_LOCAL_TWO_PHASE_TESTSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_LOCAL_TWO_PHASE_TESTSERVER_H_

#include <string>

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace safe_browsing {

// Runs an in-process two phase upload test server.
class LocalTwoPhaseTestServer {
 public:
  // Initialize a two phase protocol test server.
  LocalTwoPhaseTestServer();

  LocalTwoPhaseTestServer(const LocalTwoPhaseTestServer&) = delete;
  LocalTwoPhaseTestServer& operator=(const LocalTwoPhaseTestServer&) = delete;

  ~LocalTwoPhaseTestServer();

  GURL GetURL(const std::string& relative_path) {
    return embedded_test_server_.GetURL(relative_path);
  }

  [[nodiscard]] bool Start() { return embedded_test_server_.Start(); }

 private:
  net::EmbeddedTestServer embedded_test_server_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_LOCAL_TWO_PHASE_TESTSERVER_H_
