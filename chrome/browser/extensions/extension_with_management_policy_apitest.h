// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"

struct ManagementPolicyRequestLog {
  std::string all_headers;
  std::string host;
};

// The ExtensionSettings policy affects host permissions which impacts several
// API integration tests. This class enables easy declaration of
// ExtensionSettings policies and functions commonly used during these tests.
class ExtensionApiTestWithManagementPolicy
    : public extensions::ExtensionApiTest {
 public:
  explicit ExtensionApiTestWithManagementPolicy(
      ContextType context_type = ContextType::kFromManifest);

  ExtensionApiTestWithManagementPolicy(
      const ExtensionApiTestWithManagementPolicy&) = delete;
  ExtensionApiTestWithManagementPolicy& operator=(
      const ExtensionApiTestWithManagementPolicy&) = delete;

  ~ExtensionApiTestWithManagementPolicy() override;
  void SetUp() override;
  void SetUpOnMainThread() override;

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  bool BrowsedTo(const std::string& test_host);
  void ClearRequestLog();
  void MonitorRequestHandler(const net::test_server::HttpRequest& request);

 private:
  std::vector<ManagementPolicyRequestLog> request_log_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WITH_MANAGEMENT_POLICY_APITEST_H_
