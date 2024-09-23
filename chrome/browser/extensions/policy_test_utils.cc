// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_test_utils.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace extensions {

namespace policy_test_utils {

namespace {

constexpr char kFileNameToIntercept[] = "update_manifest.xml";

// Replace "mock.http" with "127.0.0.1:<port>" on "update_manifest.xml" files.
// Host resolver doesn't work here because the test file doesn't know the
// correct port number.
std::unique_ptr<net::test_server::HttpResponse> InterceptMockHttp(
    net::EmbeddedTestServer* embedded_test_server,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().ExtractFileName() != kFileNameToIntercept) {
    return nullptr;
  }

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  // Remove the leading '/'.
  std::string relative_manifest_path = request.GetURL().path().substr(1);
  std::string manifest_response;
  CHECK(base::ReadFileToString(test_data_dir.Append(relative_manifest_path),
                               &manifest_response));

  base::ReplaceSubstringsAfterOffset(
      &manifest_response, 0, "mock.http",
      embedded_test_server->host_port_pair().ToString());

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/xml");
  response->set_content(manifest_response);
  return response;
}

}  // namespace

void SetUpEmbeddedTestServer(net::EmbeddedTestServer* embedded_test_server) {
  embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&InterceptMockHttp, embedded_test_server));
}

void SetExtensionInstallForcelistPolicy(
    const ExtensionId& extension_id,
    const GURL& update_manifest_url,
    Profile* profile,
    policy::MockConfigurationPolicyProvider* policy_provider) {
  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a mock URL for test with an update manifest that
  // includes the crx file of the test extension.
  base::Value::List forcelist;
  forcelist.Append(base::StringPrintf("%s;%s", extension_id.c_str(),
                                      update_manifest_url.spec().c_str()));

  policy::PolicyMap policy;
  policy.Set(policy::key::kExtensionInstallForcelist,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
             nullptr);

  // Set the policy and wait until the extension is installed.
  extensions::TestExtensionRegistryObserver observer(
      ExtensionRegistry::Get(profile));
  policy_provider->UpdateChromePolicy(policy);
  observer.WaitForExtensionLoaded();
}

}  // namespace policy_test_utils

}  // namespace extensions
