// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_RECOVERY_SERVICE_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_RECOVERY_SERVICE_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

#include "base/memory/raw_ptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

class FakeRecoveryServiceMixin : public InProcessBrowserTestMixin {
 public:
  FakeRecoveryServiceMixin(InProcessBrowserTestMixinHost* host,
                           net::EmbeddedTestServer* test_server);

  FakeRecoveryServiceMixin(const FakeRecoveryServiceMixin&) = delete;
  FakeRecoveryServiceMixin& operator=(const FakeRecoveryServiceMixin&) = delete;

  ~FakeRecoveryServiceMixin() override;

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Configures the test server to answer with HTTP status code
  // |http_status_code| and an empty body when |request_path| is requeqsted.
  void SetErrorResponse(std::string request_path,
                        net::HttpStatusCode http_status_code);

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  raw_ptr<net::EmbeddedTestServer> test_server_;

  using ErrorResponseMap = base::flat_map<std::string, net::HttpStatusCode>;
  ErrorResponseMap error_responses_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_RECOVERY_SERVICE_MIXIN_H_
