// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains non-UI interactive tests for the extensions commands API.
// For UI interactive tests, see extension_keybinding_apitest.cc.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using CommandsBrowserTest = ExtensionApiTest;

// This extension ID is used for tests require a stable ID over multiple
// extension installs.
const char kId[] = "pgoakhfeplldmjheffidklpoklkppipp";

// Default keybinding to use for emulating user-defined shortcut overrides. The
// test extensions use Alt+Shift+F and Alt+Shift+H.
const char kAltShiftG[] = "Alt+Shift+G";

// Named command for media key overwrite test.
const char kMediaKeyTestCommand[] = "test_mediakeys_update";

// Given an |action_type|, returns the corresponding command key.
const char* GetCommandKeyForActionType(ActionInfo::Type action_type) {
  const char* command_key = nullptr;
  switch (action_type) {
    case ActionInfo::Type::kBrowser:
      command_key = manifest_values::kBrowserActionCommandEvent;
      break;
    case ActionInfo::Type::kPage:
      command_key = manifest_values::kPageActionCommandEvent;
      break;
    case ActionInfo::Type::kAction:
      command_key = manifest_values::kActionCommandEvent;
      break;
  }

  return command_key;
}

}  // namespace

// A parameterized version to allow testing with different action types.
class ActionCommandsBrowserTest
    : public CommandsBrowserTest,
      public testing::WithParamInterface<ActionInfo::Type> {};

// This test validates that the getAll query API function returns registered
// commands as well as synthesized ones and that inactive commands (like the
// synthesized ones are in nature) have no shortcuts.
IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, SynthesizedCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/synthesized")) << message_;
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, PageActionKeyUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(profile());
  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kActionCommandEvent, kAltShiftG);

  // Verify that the keybinding has been updated.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(extension->id(),
                              manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

// Verify that keyboard shortcut takes effect without reloading the extension.
// Regression test for https://crbug.com/1190476.
IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, ActionKeyUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  // Simulate the user changing the keybinding.
  CommandService* command_service = CommandService::Get(profile());
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kActionCommandEvent, "Ctrl+Shift+Y");

  // Verify that the keybinding has been updated.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(extension->id(),
                              manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_Y, accelerator.key_code());
  EXPECT_TRUE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_FALSE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, ShortcutAddedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v1_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v1_unassigned.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v2"),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it is set to nothing.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, ShortcutChangedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_reassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_reassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+J.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_J, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, ShortcutRemovedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_unassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify the keybinding gets set to nothing.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest,
                       ShortcutAddedOnUpdateAfterBeingAssignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v1_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v1_unassigned.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v2"),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it is set to nothing.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kActionCommandEvent, kAltShiftG);

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify the previously-set keybinding is still set.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest,
                       ShortcutChangedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_reassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_reassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kActionCommandEvent, kAltShiftG);

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+G.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

// Test that Media keys do not overwrite previous settings.
IN_PROC_BROWSER_TEST_F(
    CommandsBrowserTest,
    MediaKeyShortcutChangedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("mk_v1"),
      scoped_temp_dir.GetPath().AppendASCII("mk_v1.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("mk_v2"),
      scoped_temp_dir.GetPath().AppendASCII("mk_v2.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of MediaPlayPause.
  ui::Accelerator accelerator =
      command_service->FindCommandByName(kId, kMediaKeyTestCommand)
          .accelerator();
  EXPECT_EQ(ui::VKEY_MEDIA_PLAY_PAUSE, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_FALSE(accelerator.IsShiftDown());
  EXPECT_FALSE(accelerator.IsAltDown());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(kId, kMediaKeyTestCommand, kAltShiftG);

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+G.
  accelerator = command_service->FindCommandByName(kId, kMediaKeyTestCommand)
                    .accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

// Make sure component extensions retain keybindings after removal then
// re-adding.
IN_PROC_BROWSER_TEST_F(CommandsBrowserTest, AddRemoveAddComponentExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      RunExtensionTest("keybinding/component", {}, {.load_as_component = true}))
      << message_;

  extensions::ComponentLoader::Get(profile())->Remove(
      "pkplfbidichfdicaijlchgnapepdginl");

  ASSERT_TRUE(
      RunExtensionTest("keybinding/component", {}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CommandsBrowserTest,
                       ShortcutRemovedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_unassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  CommandService* command_service = CommandService::Get(profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user reassigning the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kActionCommandEvent, kAltShiftG);

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify the keybinding is still set.
  accelerator =
      command_service
          ->FindCommandByName(kId, manifest_values::kActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

// This test validates that commands.getAll() returns commands associated with
// a registered [page/browser] action.
IN_PROC_BROWSER_TEST_P(ActionCommandsBrowserTest, GetAllReturnsActionCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const ActionInfo::Type action_type = GetParam();

  // Load a test extension that has a command for the current action type.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Extension Commands Get All Test",
      "manifest_version": %d,
      "version": "0.1",
      "commands": {
        "%s": {
          "suggested_key": {
            "default": "Ctrl+Shift+5"
          }
        }
      },
      "%s": {},
      "background": { %s }
    }
  )";
  constexpr char kBackgroundScriptTemplate[] = R"(
      var platformBinding =
        /Mac/.test(navigator.platform) ? '⇧⌘5' : 'Ctrl+Shift+5';
      chrome.commands.getAll(function(commands) {
        chrome.test.assertEq(1, commands.length);

        chrome.test.assertEq("%s",            commands[0].name);
        chrome.test.assertEq("",              commands[0].description);
        chrome.test.assertEq(platformBinding, commands[0].shortcut);

        chrome.test.notifyPass();
      });
  )";
  const char* background_specification =
      action_type == ActionInfo::Type::kAction
          ? R"("service_worker": "background.js")"
          : R"("scripts": ["background.js"])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(action_type),
      GetCommandKeyForActionType(action_type),
      ActionInfo::GetManifestKeyForActionType(action_type),
      background_specification));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundScriptTemplate,
                         GetCommandKeyForActionType(action_type)));

  EXPECT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActionCommandsBrowserTest,
                         testing::Values(ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kAction));

}  // namespace extensions
