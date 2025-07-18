// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

constexpr char kManifest[] =
    R"json(
{
  "key": "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAx4knjNdvmPkhkdExixnuMgINyVAB1bUL38VfuoSebNeDrwLQah3fe8eeUhO6VYtYGFgEUpNvq/fywO1MiS7cENVsdrtM6gSYr1RdJG8zKwSgf9IKUQjevV9aflpgUwSrW8v6oGLkT+MLzkEgV9J1FKqb9kcSuoP2GRsPqAH5igJFTZ9nP9IS0F6p7/oZ641CjugjnIhCK2uaqNrINU1CmcRzLoOBRrtXYuawCdXvrPBM+5b9NxTKeF3JDYoAiLt2SYWRbaT2Mwys8TZPpPBYa6/QZdt7XPWo3PQZap0Ei2oYVEedDQ1IcnoP6ZFG3yJmSiADah/8rZG06fnkyERcNQIDAQAB",
  "name": "A test extension for experimentalActor API",
  "version": "0.0.1",
  "manifest_version": 3,
  "background": {
    "service_worker": "test.js"
  },
  "permissions": [
    "experimentalActor",
    "tabs",
    "windows"
  ]
}
)json";

constexpr char kTestJs[] = R"(
var availableTests = [
  async function createAndNavigateTab() {
    const initialTabs = await chrome.tabs.query({});
    const urlToNavigate = 'about:blank';

    // 1. Create a task.
    const taskId = await chrome.experimentalActor.createTask();
    chrome.test.assertTrue(!!taskId, 'Task ID should be non-zero.');

    // 2. Create a new tab using performActions.
    const createTabAction =
        new proto.chrome_intelligence_proto_features.CreateTabAction();
    // We need a window to create a tab in. Get the current window.
    const currentWindow = await chrome.windows.getCurrent();
    createTabAction.setWindowId(currentWindow.id);
    createTabAction.setForeground(true);

    const createTab = new proto.chrome_intelligence_proto_features.Action();
    createTab.setCreateTab(createTabAction);

    const actionsToCreateTab =
        new proto.chrome_intelligence_proto_features.Actions();
    actionsToCreateTab.setTaskId(taskId);
    actionsToCreateTab.addActions(createTab);

    const actionsToCreateTabProto = actionsToCreateTab.serializeBinary();
    await chrome.experimentalActor.performActions(
        actionsToCreateTabProto.buffer);

    // 3. Check that a new tab was created and get its ID.
    const tabsAfterAction = await chrome.tabs.query({});
    chrome.test.assertEq(
        initialTabs.length + 1, tabsAfterAction.length,
        'A new tab should have been created.');
    const initialTabIds = initialTabs.map(t => t.id);
    const tabsAfterActionIds = tabsAfterAction.map(t => t.id);
    const newTabIds =
        tabsAfterActionIds.filter(id => !initialTabIds.includes(id));
    chrome.test.assertEq(
        1, newTabIds.length,
        'Exactly one new tab should have been created.');
    const newTabId = newTabIds[0];
    chrome.test.assertTrue(!!newTabId, 'New Tab ID should be non-zero.');

    // 4. Navigate the new tab to the desired URL.
    const navigateAction =
        new proto.chrome_intelligence_proto_features.NavigateAction();
    navigateAction.setTabId(newTabId);
    navigateAction.setUrl(urlToNavigate);

    const action = new proto.chrome_intelligence_proto_features.Action();
    action.setNavigate(navigateAction);

    const actions = new proto.chrome_intelligence_proto_features.Actions();
    actions.setTaskId(taskId);
    actions.addActions(action);

    const actionsProto = actions.serializeBinary();
    await chrome.experimentalActor.performActions(actionsProto.buffer);

    // 5. Verify the tab was navigated.
    const navigatedTab = await chrome.tabs.get(newTabId);
    chrome.test.assertEq(urlToNavigate, navigatedTab.url);

    // 6. Final check on tab count.
    const finalTabs = await chrome.tabs.query({});
    chrome.test.assertEq(
        initialTabs.length + 1, finalTabs.length,
        'Should have one new tab');

    chrome.test.succeed();
  },
];
chrome.test.runTests(availableTests);
)";

}  // namespace

class ExtensionApiTestWithFlags : public ExtensionApiTest {
 public:
 void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExtensionExperimentalActor);
 }
};

// TODO(crbug.com/421441072): This test is intentionally disabled for now
// because there are currently no public js-bindings for edition protobuf files.
// The previous test did not test any relevant production logic.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithFlags,
                       DISABLED_ExtensionExperimentalActor) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJs);
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
