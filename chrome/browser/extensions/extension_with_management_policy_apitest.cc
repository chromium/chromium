// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"

ExtensionApiTestWithManagementPolicy::ExtensionApiTestWithManagementPolicy(
    ContextType context_type)
    : ExtensionApiTest(context_type) {}
ExtensionApiTestWithManagementPolicy::~ExtensionApiTestWithManagementPolicy() =
    default;

void ExtensionApiTestWithManagementPolicy::SetUp() {
  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &ExtensionApiTestWithManagementPolicy::MonitorRequestHandler,
      base::Unretained(this)));
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
  extensions::ExtensionApiTest::SetUp();
}

void ExtensionApiTestWithManagementPolicy::SetUpOnMainThread() {
  extensions::ExtensionApiTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
}

void ExtensionApiTestWithManagementPolicy::MonitorRequestHandler(
    const net::test_server::HttpRequest& request) {
  auto host = request.headers.find("Host");
  if (host != request.headers.end()) {
    ManagementPolicyRequestLog log;
    size_t delimiter_pos = host->second.find(":");
    log.host = host->second.substr(0, delimiter_pos);
    request_log_.push_back(log);
  }
}

bool ExtensionApiTestWithManagementPolicy::BrowsedTo(
    const std::string& test_host) {
  return base::Contains(request_log_, test_host,
                        &ManagementPolicyRequestLog::host);
}

void ExtensionApiTestWithManagementPolicy::ClearRequestLog() {
  request_log_.clear();
}
