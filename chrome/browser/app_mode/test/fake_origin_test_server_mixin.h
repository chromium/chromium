// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODE_TEST_FAKE_ORIGIN_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_APP_MODE_TEST_FAKE_ORIGIN_TEST_SERVER_MIXIN_H_

#include <cstddef>
#include <string_view>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace ash {

// This mixin sets up an `EmbeddedTestServer` configured to serve the files in
// Chrome's src/`path_to_be_served` under the given `origin`.
//
// This allows tests to serve content on a "real" origin like
// "http://your.app.com", instead of "http://127.0.0.1:<port>" with a varying
// port number.
//
// Both HTTP and HTTPS `origin` schemes are supported.
class FakeOriginTestServerMixin : public InProcessBrowserTestMixin {
 public:
  FakeOriginTestServerMixin(InProcessBrowserTestMixinHost* host,
                            GURL origin,
                            base::FilePath::StringPieceType path_to_be_served);
  FakeOriginTestServerMixin(const FakeOriginTestServerMixin&) = delete;
  FakeOriginTestServerMixin operator=(const FakeOriginTestServerMixin&) =
      delete;

  ~FakeOriginTestServerMixin() override;

  const GURL& origin() const { return origin_; }

  // Returns the `origin_` URL with `url_suffix` appended to it. For example
  // given `url_suffix` is `/path?q=123` the URL would be
  // "https://foo.com/path?q=123".
  //
  // `url_suffix` must start with "/".
  GURL GetUrl(std::string_view url_suffix) const;

  net::test_server::EmbeddedTestServer& server() { return server_; }

  const net::test_server::EmbeddedTestServer& server() const { return server_; }

  // InProcessBrowserTestMixin overrides:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  // The origin to be served. Must be a URL with scheme and host, optionally
  // followed by a port and a trailing slash.
  //
  // For example, "https://foo.com/" and "http://127.0.0.1:3244" are allowed but
  // "https://foo.com/bar" and "http://127.0.0.1:3244?q=123" are not.
  GURL origin_;

  // Path to the directory to be served, relative to Chrome's src/ directory.
  base::FilePath path_to_be_served_;

  net::test_server::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_APP_MODE_TEST_FAKE_ORIGIN_TEST_SERVER_MIXIN_H_
