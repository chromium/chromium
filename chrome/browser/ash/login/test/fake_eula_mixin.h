// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_EULA_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_EULA_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

// Mixin that serves fake eula for OOBE.
class FakeEulaMixin : public InProcessBrowserTestMixin {
 public:
  static const char* kFakeOnlineEula;
  static const char* kOfflineEULAWarning;

  FakeEulaMixin(InProcessBrowserTestMixinHost* host,
                net::EmbeddedTestServer* test_server);

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  ~FakeEulaMixin() override;

  // Used for customizing the response handler of the embedded server.
  void set_force_http_unavailable(bool force_unavailable) {
    force_http_unavailable_ = force_unavailable;
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
  raw_ptr<net::EmbeddedTestServer> test_server_;

  // The default behaviour for the embedded server is to service the
  // online version properly. Offline tests may change this during construction
  // of the class.
  bool force_http_unavailable_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_EULA_MIXIN_H_
