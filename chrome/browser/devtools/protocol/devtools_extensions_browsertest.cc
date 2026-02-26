// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window_deleter.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
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

  const base::DictValue* SendLoadUnpackedCommand(
      const std::string& path,
      bool enable_in_incognito = false) {
    base::FilePath extension_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII("devtools")
            .AppendASCII("extensions")
            .AppendASCII(path);

    base::DictValue params;
    params.Set("path", extension_path.AsUTF8Unsafe());
    params.Set("enableInIncognito", enable_in_incognito);

    return SendCommandSync("Extensions.loadUnpacked", std::move(params));
  }

  scoped_refptr<const extensions::Extension> InstallExtensionFromPath(
      const std::string& path) {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());

    base::FilePath extension_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII("devtools")
            .AppendASCII("extensions")
            .AppendASCII(path);

    return loader.LoadExtension(extension_path);
  }

  const base::DictValue* SendStorageCommand(
      const std::string& command,
      const extensions::Extension* extension,
      base::DictValue extra_params) {
    base::DictValue storage_params;
    storage_params.Set("id", extension->id());
    storage_params.Set("storageArea", "local");
    storage_params.Merge(std::move(extra_params));

    const base::DictValue* get_result =
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

  base::DictValue params;
  params.Set("id", id);
  const base::DictValue* uninstall_result =
      SendCommandSync("Extensions.uninstall", std::move(params));
  ASSERT_FALSE(uninstall_result);

  const extensions::Extension* extension_after =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_after);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanInstallExtension) {
  const base::DictValue* result =
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
  ASSERT_FALSE(extensions::util::IsIncognitoEnabled(*result->FindString("id"),
                                                    browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanInstallExtensionAndEnableItForIncognito) {
  const base::DictValue* result =
      SendLoadUnpackedCommand("simple_background_page", true);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->FindString("id"));
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(*result->FindString("id"));
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(),
            extensions::mojom::ManifestLocation::kUnpacked);

  ASSERT_TRUE(extensions::util::IsIncognitoEnabled(*result->FindString("id"),
                                                   browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       ThrowsOnWrongPath) {
  const base::DictValue* result = SendLoadUnpackedCommand("non-existent");
  ASSERT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       InstalledExtensionIsNotEnabledInIncognito) {
  const base::DictValue* result = SendLoadUnpackedCommand(
      "simple_background_page", /*enable_in_incognito=*/false);
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
  // Verify that the extension is not enabled in incognito, as `false` was
  // passed for `enable_in_incognito`.
  ASSERT_FALSE(extensions::util::IsIncognitoEnabled(*result->FindString("id"),
                                                    browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanUninstallExtension) {
  const base::DictValue* install_result =
      SendLoadUnpackedCommand("simple_background_page");

  std::string id = *install_result->FindString("id");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  const extensions::Extension* extension_before =
      registry->GetInstalledExtension(id);
  ASSERT_TRUE(extension_before);

  base::DictValue params;
  params.Set("id", id);
  const base::DictValue* uninstall_result =
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

  base::DictValue params;
  params.Set("id", id);
  const base::DictValue* uninstall_result =
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

  base::DictValue params;
  params.Set("id", id);
  const base::DictValue* uninstallResult =
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
        host->GetURL().GetHost() == id) {
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
    if (host->GetType() == "background_page" &&
        host->GetURL().GetPath() == path) {
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
        host->GetURL().GetPath() == path) {
      return host;
    }
  }
  return nullptr;
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValues) {
  ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");

  const base::DictValue* load_result =
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
      base::DictValue().Set("values", base::DictValue()
                                          .Set("foo", "bar")
                                          .Set("other", "value")
                                          .Set("remove-on-clear", "value"))));

  // Check only the requested keys are returned.
  const base::DictValue* get_result = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::DictValue().Set("keys", base::ListValue().Append("foo")));
  ASSERT_TRUE(get_result);
  ASSERT_EQ(*get_result->FindDict("data")->FindString("foo"), "bar");
  ASSERT_FALSE(get_result->FindDict("data")->contains("other"));

  // Remove the `foo` key.
  ASSERT_TRUE(SendStorageCommand(
      "Extensions.removeStorageItems", extension,
      base::DictValue().Set("keys", base::ListValue().Append("foo"))));

  // Check the `foo` key no longer exists.
  const base::DictValue* get_result_2 = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::DictValue().Set("keys", base::ListValue().Append("foo")));
  ASSERT_TRUE(get_result_2);
  ASSERT_FALSE(get_result_2->FindDict("data")->contains("foo"));

  // Clear the storage area.
  ASSERT_TRUE(SendStorageCommand("Extensions.clearStorageItems", extension,
                                 base::DictValue()));

  // Check the `remove-on-clear` key no longer exists.
  const base::DictValue* get_result_3 = SendStorageCommand(
      "Extensions.getStorageItems", extension,
      base::DictValue().Set("keys",
                            base::ListValue().Append("remove-on-clear")));
  ASSERT_TRUE(get_result_3);
  ASSERT_FALSE(get_result_3->FindDict("data")->contains("remove-on-clear"));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValuesBackgroundPage) {
  const base::DictValue* load_result =
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
                                 base::DictValue()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetStorageValuesContentScript) {
  const base::DictValue* load_result =
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
                                 base::DictValue()));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CannotGetStorageValuesWithoutContentScript) {
  // Load an extension with no associated content scripts.
  const base::DictValue* load_result =
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

  const base::DictValue* get_result = SendStorageCommand(
      "Extensions.getStorageItems", extension, base::DictValue());

  // Command should fail as extension has not injected content script.
  EXPECT_FALSE(get_result);
  ASSERT_EQ(*error()->FindString("message"), "Extension not found.");
}

// Test to ensure that the target associated with an extension service worker
// cannot access data from the storage associated with another extension.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CannotGetStorageValuesUnrelatedTarget) {
  ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");

  const base::DictValue* load_result =
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
  base::DictValue storage_params;
  storage_params.Set("id", second_extension_id);
  storage_params.Set("storageArea", "local");

  const base::DictValue* get_result =
      SendCommandSync("Extensions.getStorageItems", std::move(storage_params));

  // Command should fail as target does not have access.
  EXPECT_FALSE(get_result);
  ASSERT_EQ(*error()->FindString("message"), "Extension not found.");
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       CanGetExtensions) {
  base::FilePath extensions_dir =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions");

  base::FilePath unpacked_path =
      extensions_dir.AppendASCII("simple_background_page");
  base::FilePath packed_path = extensions_dir.AppendASCII("service_worker");

  // Load an unpacked extension.
  const base::DictValue* result =
      SendLoadUnpackedCommand("simple_background_page");
  std::string id = *result->FindString("id");
  ASSERT_FALSE(id.empty());

  // Load packed extension
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.set_location(extensions::mojom::ManifestLocation::kInternal);
  loader.set_pack_extension(true);
  auto packed_extension = loader.LoadExtension(packed_path);
  ASSERT_TRUE(packed_extension);
  std::string packed_id = packed_extension->id();
  ASSERT_FALSE(packed_id.empty());

  // Verify the internal extension is actually in the registry.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_TRUE(registry->enabled_extensions().Contains(packed_id));

  content::RunAllTasksUntilIdle();
  const base::DictValue* list_result =
      SendCommandSync("Extensions.getExtensions", base::DictValue());
  ASSERT_TRUE(list_result);

  const base::ListValue* extensions = list_result->FindList("extensions");
  ASSERT_TRUE(extensions);
  EXPECT_EQ(extensions->size(), 1u);

  const base::DictValue& extension_info = (*extensions)[0].GetDict();
  EXPECT_EQ(*extension_info.FindString("id"), id);
  EXPECT_EQ(*extension_info.FindString("name"),
            "Test Extension - Simple Background Page");
  EXPECT_EQ(*extension_info.FindString("version"), "0.1");
  EXPECT_TRUE(*extension_info.FindBool("enabled"));
  EXPECT_EQ(*extension_info.FindStringByDottedPath("path"),
            unpacked_path.AsUTF8Unsafe());
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       TriggerActionShowsSidePanel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ExtensionTestMessageListener activated_listener("running");
  scoped_refptr<const extensions::Extension> extension =
      InstallExtensionFromPath("side_panel_action");
  ASSERT_TRUE(activated_listener.WaitUntilSatisfied());

  extensions::ResultCatcher result_catcher;
  scoped_refptr<content::DevToolsAgentHost> page_host =
      content::DevToolsAgentHost::GetOrCreateForTab(
          browser()->tab_strip_model()->GetActiveWebContents());
  base::DictValue trigger_extension_params;
  trigger_extension_params.Set("id", extension->id());
  trigger_extension_params.Set("targetId", page_host->GetId());
  const base::DictValue* trigger_result = SendCommandSync(
      "Extensions.triggerAction", std::move(trigger_extension_params));
  ASSERT_TRUE(trigger_result);

  EXPECT_FALSE(trigger_result->FindDict("error"));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  SidePanelUI* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_TRUE(side_panel_ui);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id())));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       TriggerActionShowsPopup) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  extensions::ResultCatcher result_catcher;
  scoped_refptr<const extensions::Extension> extension =
      InstallExtensionFromPath("popup_action");

  scoped_refptr<content::DevToolsAgentHost> page_host =
      content::DevToolsAgentHost::GetOrCreateForTab(
          browser()->tab_strip_model()->GetActiveWebContents());
  base::DictValue trigger_extension_params;
  trigger_extension_params.Set("id", extension->id());
  trigger_extension_params.Set("targetId", page_host->GetId());
  const base::DictValue* trigger_result = SendCommandSync(
      "Extensions.triggerAction", std::move(trigger_extension_params));
  ASSERT_TRUE(trigger_result);

  EXPECT_FALSE(trigger_result->FindDict("error"));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  BrowserWindowInterface* bwi =
      extensions::browser_window_util::GetBrowserForTabContents(
          *browser()->tab_strip_model()->GetActiveWebContents());
  auto* extensions_container = ExtensionsContainer::From(*bwi);

  ASSERT_TRUE(extensions_container);
  auto* action_view = extensions_container->GetActionForId(extension->id());
  ASSERT_TRUE(action_view);
  EXPECT_TRUE(action_view->IsShowingPopup());
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolWithUnsafeDebuggingTest,
                       TriggerActionDispatchesEvent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  extensions::ResultCatcher result_catcher;

  scoped_refptr<const extensions::Extension> extension =
      InstallExtensionFromPath("on_clicked_action");

  scoped_refptr<content::DevToolsAgentHost> page_host =
      content::DevToolsAgentHost::GetOrCreateForTab(
          browser()->tab_strip_model()->GetActiveWebContents());

  base::DictValue trigger_extension_params;
  trigger_extension_params.Set("id", extension->id());
  trigger_extension_params.Set("targetId", page_host->GetId());
  const base::DictValue* trigger_result = SendCommandSync(
      "Extensions.triggerAction", std::move(trigger_extension_params));
  ASSERT_TRUE(trigger_result);

  EXPECT_FALSE(trigger_result->FindDict("error"));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}
}  // namespace
