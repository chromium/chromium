// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_CONNECT_PROXY_H_
#define CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_CONNECT_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "net/base/host_port_pair.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace enterprise::test {

// Thin thread-safe wrapper around `net::EmbeddedTestServer`'s built-in
// `EnableConnectProxy()` and `RegisterAuthHandler()` for testing HTTP 407
// challenges, disguised error realms, and multi-proxy failover.
class TestConnectProxy {
 public:
  TestConnectProxy(
      std::string hostname,
      std::vector<net::HostPortPair> proxied_destinations,
      std::string default_challenge_realm = "cloud_secure_gateway");
  TestConnectProxy(const TestConnectProxy&) = delete;
  TestConnectProxy& operator=(const TestConnectProxy&) = delete;
  ~TestConnectProxy();

  // Initializes the listening socket, configures `EnableConnectProxy()` for
  // `proxied_destinations_`, and starts accepting connections.
  bool Start();

  // Shuts down the listening proxy server (useful for failover testing).
  bool Shutdown();

  net::HostPortPair host_port_pair() const {
    return net::HostPortPair(hostname_, server_.port());
  }

  GURL GetURL() const { return server_.GetURL(hostname_, "/"); }

  void SetRequireAuth(bool require_auth);
  void SetChallengeRealm(std::string realm);
  void ResetStats();

  bool was_accessed() const;
  bool was_auth_header_received() const;
  std::string last_received_auth_header() const;
  std::vector<std::string> tunneled_targets() const;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleAuthRequest(
      const net::test_server::HttpRequest& request);

  const std::string hostname_;
  const std::vector<net::HostPortPair> proxied_destinations_;

  mutable base::Lock lock_;
  bool require_auth_ GUARDED_BY(lock_) = true;
  std::string challenge_realm_ GUARDED_BY(lock_);
  std::string last_received_auth_header_ GUARDED_BY(lock_);
  std::vector<std::string> tunneled_targets_ GUARDED_BY(lock_);

  // Declared last so `~EmbeddedTestServer()` joins `io_thread_` before `lock_`
  // and `GUARDED_BY(lock_)` fields accessed by `HandleAuthRequest()` are
  // destroyed.
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_CONNECT_PROXY_H_
