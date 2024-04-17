// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/path_service.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace {

class DevToolsExtensionsProtocolDisabledTest : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    AttachToBrowserTarget();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTestBase::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(::switches::kEnableUnsafeExtensionDebugging);
  }
};

class DevToolsExtensionsProtocolTest
    : public DevToolsExtensionsProtocolDisabledTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsExtensionsProtocolDisabledTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableUnsafeExtensionDebugging);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolDisabledTest,
                       CannotInstallExtension) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("simple_background_page");
  base::Value::Dict params;
  params.Set("path", extension_path.AsUTF8Unsafe());
  const base::Value::Dict* result =
      SendCommandSync("Extensions.loadUnpacked", std::move(params));
  ASSERT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolTest, CanInstallExtension) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("simple_background_page");
  base::Value::Dict params;
  params.Set("path", extension_path.AsUTF8Unsafe());
  const base::Value::Dict* result =
      SendCommandSync("Extensions.loadUnpacked", std::move(params));
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

IN_PROC_BROWSER_TEST_F(DevToolsExtensionsProtocolTest, ThrowsOnWrongPath) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("non-existent");
  base::Value::Dict params;
  params.Set("path", extension_path.AsUTF8Unsafe());
  const base::Value::Dict* result =
      SendCommandSync("Extensions.loadUnpacked", std::move(params));
  ASSERT_FALSE(result);
}

}  // namespace
