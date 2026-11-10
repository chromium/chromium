// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/test/test_connect_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise::test {

TestConnectProxy::TestConnectProxy(
    std::string hostname,
    std::vector<net::HostPortPair> proxied_destinations,
    std::string default_challenge_realm)
    : hostname_(std::move(hostname)),
      proxied_destinations_(std::move(proxied_destinations)),
      challenge_realm_(std::move(default_challenge_realm)) {
  server_.SetCertHostnames({hostname_});
  server_.RegisterAuthHandler(base::BindRepeating(
      &TestConnectProxy::HandleAuthRequest, base::Unretained(this)));
}

TestConnectProxy::~TestConnectProxy() {
  if (server_.Started()) {
    EXPECT_TRUE(server_.ShutdownAndWaitUntilComplete());
  }
}

bool TestConnectProxy::Start() {
  if (!server_.InitializeAndListen()) {
    return false;
  }
  server_.EnableConnectProxy(proxied_destinations_);
  server_.StartAcceptingConnections();
  return true;
}

bool TestConnectProxy::Shutdown() {
  return server_.ShutdownAndWaitUntilComplete();
}

void TestConnectProxy::SetRequireAuth(bool require_auth) {
  base::AutoLock auto_lock(lock_);
  require_auth_ = require_auth;
}

void TestConnectProxy::SetChallengeRealm(std::string realm) {
  base::AutoLock auto_lock(lock_);
  challenge_realm_ = std::move(realm);
}

void TestConnectProxy::ResetStats() {
  base::AutoLock auto_lock(lock_);
  last_received_auth_header_.clear();
  tunneled_targets_.clear();
}

bool TestConnectProxy::was_accessed() const {
  base::AutoLock auto_lock(lock_);
  return !tunneled_targets_.empty();
}

bool TestConnectProxy::was_auth_header_received() const {
  base::AutoLock auto_lock(lock_);
  return !last_received_auth_header_.empty();
}

std::string TestConnectProxy::last_received_auth_header() const {
  base::AutoLock auto_lock(lock_);
  return last_received_auth_header_;
}

std::vector<std::string> TestConnectProxy::tunneled_targets() const {
  base::AutoLock auto_lock(lock_);
  return tunneled_targets_;
}

std::unique_ptr<net::test_server::HttpResponse>
TestConnectProxy::HandleAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_CONNECT) {
    return nullptr;
  }

  base::AutoLock auto_lock(lock_);
  tunneled_targets_.push_back(request.relative_url);

  if (!require_auth_) {
    return nullptr;
  }

  auto auth_it = request.headers.find("Proxy-Authorization");
  if (auth_it == request.headers.end()) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
    response->AddCustomHeader(
        "Proxy-Authenticate",
        base::StringPrintf("Basic realm=\"%s\"", challenge_realm_.c_str()));
    response->set_content("Proxy Authentication Required");
    response->set_content_type("text/plain");
    return response;
  }

  last_received_auth_header_ = auth_it->second;
  return nullptr;
}

}  // namespace enterprise::test
