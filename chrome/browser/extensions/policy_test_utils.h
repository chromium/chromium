// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_TEST_UTILS_H_

#include "extensions/common/extension_id.h"

class GURL;
class Profile;

namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

namespace policy {
class MockConfigurationPolicyProvider;
}  // namespace policy

namespace extensions {

namespace policy_test_utils {

// Intercepts "update_manifest.xml" files requests.
void SetUpEmbeddedTestServer(
    net::test_server::EmbeddedTestServer* embedded_test_server);

// Assigns an |extension_id| and its |update_manifest_url| to the
// "ExtensionInstallForcelist" user policy.
// This will cause the extension to get force-installed.
void SetExtensionInstallForcelistPolicy(
    const ExtensionId& extension_id,
    const GURL& update_manifest_url,
    Profile* profile,
    policy::MockConfigurationPolicyProvider* policy_provider);

}  // namespace policy_test_utils

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_TEST_UTILS_H_
