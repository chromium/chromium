// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

class DevToolsExtensionsProtocolTest : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    AttachToBrowserTarget();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTestBase::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(::switches::kEnableUnsafeExtensionDebugging);
  }

  const base::Value::Dict* SendLoadUnpackedCommand(const std::string& path) {
    base::FilePath extension_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII("devtools")
            .AppendASCII("extensions")
            .AppendASCII(path);

    base::Value::Dict params;
    params.Set("path", extension_path.AsUTF8Unsafe());

    return SendCommandSync("Extensions.loadUnpacked", std::move(params));
  }

  const base::Value::Dict* SendStorageCommand(
      const std::string& command,
      const extensions::Extension* extension,
      base::Value::Dict extra_params) {
    base::Value::Dict storage_params;
    storage_params.Set("id", extension->id());
    storage_params.Set("storageArea", "local");
    storage_params.Merge(std::move(extra_params));

    const base::Value::Dict* get_result =
        SendCommandSync(command, std::move(storage_params));
    return get_result;
  }

 private:
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

class DevToolsExtensionsProtocolWithUnsafeDebuggingTest
    : public DevToolsExtensionsProtocolTest {
 public:
  DevToolsExtensionsProtocolWithUnsafeDebuggingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionDisableUnsupportedDeveloper);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsExtensionsProtocolTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableUnsafeExtensionDebugging);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolTest, CannotInstallExtension) {
  ASSERT_FALSE(SendLoadUnpackedCommand("simple_background_page"));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolTest,
                       CannotUninstallExtension) {
  auto extension =
      extensions::ExtensionBuilder("unpacked")
          .SetLocation(extensions::mojom::ManifestLocation::kUnpacked)
          .Build();
  extensions::ExtensionRegistrar::Get(browser()->profile())
      ->AddExtension(extension.get());

  std::string id = extension.get()->id();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  const extensions::Extension* extension_before =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_before);

  base::Value::Dict params;
  params.Set("id", id);
  const base::Value::Dict* uninstall_result =
      SendCommandSync("Extensions.uninstall", std::move(params));
  ASSERT_FALSE(uninstall_result);

  const extensions::Extension* extension_after =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_after);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanInstallExtension) {
  const base::Value::Dict* result =
      SendLoadUnpackedCommand("simple_background_page");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->FindString("id"));
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension = registry->GetExtensionById(
      *result->FindString("id"), extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), *result->FindString("id"));
  ASSERT_EQ(extension->location(),
            extensions::mojom::ManifestLocation::kUnpacked);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       ThrowsOnWrongPath) {
  const base::Value::Dict* result = SendLoadUnpackedCommand("non-existent");
  ASSERT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanUninstallExtension) {
  const base::Value::Dict* install_result =
      SendLoadUnpackedCommand("simple_background_page");

  std::string id = *install_result->FindString("id");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  const extensions::Extension* extension_before =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_before);

  base::Value::Dict params;
  params.Set("id", id);
  const base::Value::Dict* uninstall_result =
      SendCommandSync("Extensions.uninstall", std::move(params));
  ASSERT_TRUE(uninstall_result);

  const extensions::Extension* extension_after =
      registry->GetInstalledExtension(id);
  ASSERT_FALSE(extension_after);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CannotUninstallNonUnpackedExtension) {
  auto extension =
      extensions::ExtensionBuilder("unpacked")
          .SetLocation(extensions::mojom::ManifestLocation::kComponent)
          .Build();
  extensions::ExtensionRegistrar::Get(browser()->profile())
      ->AddExtension(extension.get());

  std::string id = extension.get()->id();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  const extensions::Extension* extension_before =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_before);

  base::Value::Dict params;
  params.Set("id", id);
  const base::Value::Dict* uninstall_result =
      SendCommandSync("Extensions.uninstall", std::move(params));
  ASSERT_FALSE(uninstall_result);

  const extensions::Extension* extension_after =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_after);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       FailsToUninstallNonexistentExtension) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  std::string id = "non-existent-id";
  const extensions::Extension* extension = registry->GetInstalledExtension(id);
  ASSERT_FALSE(extension);

  base::Value::Dict params;
  params.Set("id", id);
  const base::Value::Dict* uninstallResult =
      SendCommandSync("Extensions.uninstall", std::move(params));
  ASSERT_FALSE(uninstallResult);

  const extensions::Extension* extensionAfter =
      registry->GetInstalledExtension(id);
  ASSERT_FALSE(extensionAfter);
}

// Returns the `DevToolsAgentHost` associated with an extension's service
// worker if available.
scoped_refptr<content::DevToolsAgentHost> FindExtensionHost(
    const std::string& id) {
  for (auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL().host() == id) {
      return host;
    }
  }
  return nullptr;
}

// Returns the `DevToolsAgentHost` associated with an extension page if
// available.
scoped_refptr<content::DevToolsAgentHost> FindBackgroundPageHost(
    const std::string& path) {
  for (auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == "background_page" && host->GetURL().path() == path) {
      return host;
    }
  }
  return nullptr;
}

// Returns the `DevToolsAgentHost` associated with an extension page if
// available.
scoped_refptr<content::DevToolsAgentHost> FindPageHost(
    const std::string& path) {
  for (auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == content::DevToolsAgentHost::kTypePage &&
        host->GetURL().path() == path) {
      return host;
    }
  }
  return nullptr;
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValues) {
  ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");

  const base::Value::Dict* load_result =
      SendLoadUnpackedCommand("service_worker");
  ASSERT_TRUE(load_result);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension = registry->GetExtensionById(
      *load_result->FindString("id"), extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);

  // Ensure service worker has had time to initialize.
  EXPECT_TRUE(activated_listener.WaitUntilSatisfied());

  // Access to storage commands is only allowed from a target associated with
  // the extension. Attach to the extension service worker to be able to test
  // the method.
  DetachProtocolClient();
  agent_host_ = FindExtensionHost(extension->id());
  agent_host_->AttachClient(this);

  //  Set some dummy values in storage.
  ASSERT_TRUE(SendStorageCommand(
      "Extensions.setStorageItems", extension,
      base::Value::Dict().Set("values", base::Value::Dict()
                                            .Set("foo", "bar")
                                            .Set("other", "value")
                                            .Set("remove-on-clear", "value"))));

  // Check only the requested keys are returned.
  const base::Value::Dict* get_result = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::Value::Dict().Set("keys", base::Value::List().Append("foo")));
  ASSERT_TRUE(get_result);
  ASSERT_EQ(*get_result->FindDict("data")->FindString("foo"), "bar");
  ASSERT_FALSE(get_result->FindDict("data")->contains("other"));

  // Remove the `foo` key.
  ASSERT_TRUE(SendStorageCommand(
      "Extensions.removeStorageItems", extension,
      base::Value::Dict().Set("keys", base::Value::List().Append("foo"))));

  // Check the `foo` key no longer exists.
  const base::Value::Dict* get_result_2 = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::Value::Dict().Set("keys", base::Value::List().Append("foo")));
  ASSERT_TRUE(get_result_2);
  ASSERT_FALSE(get_result_2->FindDict("data")->contains("foo"));

  // Clear the storage area.
  ASSERT_TRUE(SendStorageCommand("Extensions.clearStorageItems", extension,
                                 base::Value::Dict()));

  // Check the `remove-on-clear` key no longer exists.
  const base::Value::Dict* get_result_3 = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::Value::Dict().Set("keys",
                              base::Value::List().Append("remove-on-clear")));
  ASSERT_TRUE(get_result_3);
  ASSERT_FALSE(get_result_3->FindDict("data")->contains("remove-on-clear"));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValuesBackgroundPage) {
  const base::Value::Dict* load_result =
      SendLoadUnpackedCommand("background_page_storage_access");
  ASSERT_TRUE(load_result);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension = registry->GetExtensionById(
      *load_result->FindString("id"), extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);

  DetachProtocolClient();

  extensions::ExtensionBackgroundPageWaiter(browser()->profile(), *extension)
      .WaitForBackgroundOpen();
  agent_host_ = FindBackgroundPageHost("/_generated_background_page.html");
  agent_host_->AttachClient(this);

  ASSERT_TRUE(SendStorageCommand("Extensions.getStorageItems", extension,
                                 base::Value::Dict()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValuesContentScript) {
  const base::Value::Dict* load_result =
      SendLoadUnpackedCommand("simple_content_script");
  ASSERT_TRUE(load_result);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension = registry->GetExtensionById(
      *load_result->FindString("id"), extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/devtools/page_with_content_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  DetachProtocolClient();
  agent_host_ = FindPageHost("/devtools/page_with_content_script.html");
  agent_host_->AttachClient(this);

  ASSERT_TRUE(SendStorageCommand("Extensions.getStorageItems", extension,
                                 base::Value::Dict()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CannotGetStorageValuesWithoutContentScript) {
  // Load an extension with no associated content scripts.
  const base::Value::Dict* load_result =
      SendLoadUnpackedCommand("service_worker");
  ASSERT_TRUE(load_result);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension = registry->GetExtensionById(
      *load_result->FindString("id"), extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/devtools/page_with_content_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  DetachProtocolClient();
  agent_host_ = FindPageHost("/devtools/page_with_content_script.html");
  agent_host_->AttachClient(this);

  const base::Value::Dict* get_result = SendStorageCommand(
      "Extensions.getStorageItems", extension, base::Value::Dict());

  // Command should fail as extension has not injected content script.
  EXPECT_FALSE(get_result);
  ASSERT_EQ(*error()->FindString("message"), "Extension not found.");
}

// Test to ensure that the target associated with an extension service worker
// cannot access data from the storage associated with another extension.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CannotGetStorageValuesUnrelatedTarget) {
  ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");

  const base::Value::Dict* load_result =
      SendLoadUnpackedCommand("service_worker");
  ASSERT_TRUE(load_result);

  const std::string first_extension_id = *load_result->FindString("id");

  // Ensure service worker has had time to initialize.
  EXPECT_TRUE(activated_listener.WaitUntilSatisfied());

  // Load a second extension.
  load_result = SendLoadUnpackedCommand("simple_background_page");
  ASSERT_TRUE(load_result);

  const std::string second_extension_id = *load_result->FindString("id");

  // Attach to first extension.
  DetachProtocolClient();
  agent_host_ = FindExtensionHost(first_extension_id);
  agent_host_->AttachClient(this);

  // Try to load data from the second extension from a context associated with
  // the first extension. This should be blocked.
  base::Value::Dict storage_params;
  storage_params.Set("id", second_extension_id);
  storage_params.Set("storageArea", "local");

  const base::Value::Dict* get_result =
      SendCommandSync("Extensions.getStorageItems", std::move(storage_params));

  // Command should fail as target does not have access.
  EXPECT_FALSE(get_result);
  ASSERT_EQ(*error()->FindString("message"), "Extension not found.");
}

}  // namespace
