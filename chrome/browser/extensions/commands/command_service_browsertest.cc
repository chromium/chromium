// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kBasicBrowserActionKeybinding[] = "Ctrl+Shift+F";
const char kBasicNamedKeybinding[] = "Ctrl+Shift+Y";
const char kBasicAlternateKeybinding[] = "Ctrl+Shift+G";
const char kBasicNamedCommand[] = "toggle-feature";
constexpr char kAltNKeybinding[] = "Alt+N";
constexpr char kAltZKeybinding[] = "Alt+Z";
constexpr char kExtensionId[] = "pgoakhfeplldmjheffidklpoklkppipp";
constexpr char kManifestTemplate[] = R"({
    "name": "An action to test if upgrading from MV2 to MV3 succeeds.",
    "version": "1.0",
    "manifest_version": %d,
    "commands": {
      "%s": {
        "suggested_key": "%s"
      }
    },
    "%s": {}})";

// Get another command platform, whcih is used for simulating a command has been
// assigned with a shortcut on another platform.
std::string GetAnotherCommandPlatform() {
#if BUILDFLAG(IS_WIN)
  return extensions::manifest_values::kKeybindingPlatformMac;
#elif BUILDFLAG(IS_MAC)
  return extensions::manifest_values::kKeybindingPlatformChromeOs;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return extensions::manifest_values::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return extensions::manifest_values::kKeybindingPlatformWin;
#else
  return "";
#endif
}

// Parameters passed to CommandServiceMv3UpgradeTest parameterized tests.
struct ManifestCommandTestParameters {
  const char* action_name;          // e.g. "browser_action", "page_action".
  const char* action_command_name;  // e.g. "_execute_browser_action",
                                    // "_execute_page_action".
  // e.g. the associated folder below
  // //chrome/test/data/extensions/api_test/keybinding/update/ that contains the
  // unpacked extension files for webstore and .crx installs.
  const char* action_command_mv2_extension_path;
};

}  // namespace

namespace extensions {

using CommandServiceTest = ExtensionApiTest;
// Test class for testing keybinding changes across MV2->MV3 reloads/updates.
class CommandServiceMv3UpgradeTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ManifestCommandTestParameters> {
 protected:
  // Create a new extension from a manifest for MV2 and verify its command
  // shortcuts are setup correctly.
  std::string LoadExtensionMv2(const char* suggested_keybinding);
  // Reload the extension from a manifest for MV3 and verify its command
  // shortcut has the new suggested keybinding.
  void ReloadExtensionMv3(const std::string& id,
                          const char* new_suggested_keybinding);
  // Install an extension with `id`. `install_from_crx` = false means it will
  // install from webstore.
  void InstallExtensionMv2(const std::string& id,
                           bool install_from_crx = false);
  // Update the extension with `id` (from webstore or via crx) from the
  // `mv3_extension_upgrade_folder`.
  void UpdateExtensionMv3(const std::string& id,
                          const char* mv3_extension_upgrade_folder);
  // Modify the browser action keybinding as if it was modified by the user.
  void ChangeMv2CommandKeybinding(const std::string& id,
                                  const char* new_keybinding);
  bool CommandHasAltPlusKeybinding(const std::string& id,
                                   const char* action_command_name,
                                   const ui::KeyboardCode key);
  bool CommandHasNoKeybinding(const std::string& id,
                              const char* action_command_name);
  // Checks that extension with `id` no longer has MV2 command action
  // keybindings and now has an MV3 keybinding of Alt + `keyboard_code`.
  void CheckMv3UpgradeHasExpectedKeybindings(
      const std::string& id,
      const ui::KeyboardCode keyboard_code);

  TestExtensionDir& test_dir() { return test_dir_; }
  CommandService* command_service() {
    return CommandService::Get(browser()->profile());
  }

 private:
  TestExtensionDir test_dir_;
  base::FilePath test_path_;
};

std::string CommandServiceMv3UpgradeTest::LoadExtensionMv2(
    const char* suggested_keybinding) {
  ManifestCommandTestParameters test_params = GetParam();
  test_dir().WriteManifest(
      base::StringPrintf(kManifestTemplate, 2, test_params.action_command_name,
                         suggested_keybinding, test_params.action_name));

  // Install MV2 of the extension.
  const Extension* extension = LoadExtension(
      test_dir().UnpackedPath(), {.wait_for_registration_stored = true});
  if (!extension) {
    ADD_FAILURE() << "Couldn't load extension successfully for test setup.";
  }
  const std::string unpacked_extension_id = extension->id();
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(
      unpacked_extension_id));

  // Verify it has an MV2 action command of Alt+N.
  EXPECT_TRUE(CommandHasAltPlusKeybinding(unpacked_extension_id.c_str(),
                                          test_params.action_command_name,
                                          ui::VKEY_N));
  // Verify MV3 action command is set to nothing before we upgrade.
  EXPECT_TRUE(CommandHasNoKeybinding(unpacked_extension_id.c_str(),
                                     manifest_values::kActionCommandEvent));
  return unpacked_extension_id;
}

void CommandServiceMv3UpgradeTest::ReloadExtensionMv3(
    const std::string& id,
    const char* new_suggested_keybinding) {
  test_dir().WriteManifest(base::StringPrintf(
      kManifestTemplate, 3, manifest_values::kActionCommandEvent,
      new_suggested_keybinding, manifest_keys::kAction));
  ReloadExtension(id);
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(id));
}

void CommandServiceMv3UpgradeTest::InstallExtensionMv2(const std::string& id,
                                                       bool install_from_crx) {
  // TODO(jlulejian): Try to make these test class members for less repetition.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  ManifestCommandTestParameters test_params = GetParam();
  base::FilePath path_mv2 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII(test_params.action_command_mv2_extension_path),
      scoped_temp_dir.GetPath().AppendASCII("v1.crx"), pem_path,
      base::FilePath());
  ASSERT_FALSE(path_mv2.empty());

  // Install MV2 of the extension.
  if (install_from_crx) {
    ASSERT_TRUE(InstallExtension(path_mv2, 1));
  } else {
    ASSERT_TRUE(InstallExtensionFromWebstore(path_mv2, 1));
  }
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(id));

  // Verify it has an MV2 action command of Alt+N.
  EXPECT_TRUE(CommandHasAltPlusKeybinding(id, test_params.action_command_name,
                                          ui::VKEY_N));
  // Verify MV3 action command is set to nothing before we upgrade.
  EXPECT_TRUE(CommandHasNoKeybinding(id, manifest_values::kActionCommandEvent));
}

void CommandServiceMv3UpgradeTest::UpdateExtensionMv3(
    const std::string& id,
    const char* mv3_extension_upgrade_folder) {
  // TODO(jlulejian): Try to make these test class members for less repetition.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("keybinding.pem");
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath path_mv3 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII(mv3_extension_upgrade_folder),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());
  ASSERT_FALSE(path_mv3.empty());

  // Update to MV3 with an action command instead of a browser action.
  ASSERT_TRUE(UpdateExtension(id, path_mv3, 0));
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(id));
}

void CommandServiceMv3UpgradeTest::ChangeMv2CommandKeybinding(
    const std::string& id,
    const char* new_keybinding) {
  const char* action_command_name = GetParam().action_command_name;
  Command action_command =
      command_service()->FindCommandByName(id, action_command_name);
  command_service()->UpdateKeybindingPrefs(id, action_command_name,
                                           new_keybinding);
}

void CommandServiceMv3UpgradeTest::CheckMv3UpgradeHasExpectedKeybindings(
    const std::string& id,
    const ui::KeyboardCode keyboard_code) {
  // Verify both MV2 commands are set to nothing.
  EXPECT_TRUE(
      CommandHasNoKeybinding(id, manifest_values::kBrowserActionCommandEvent));
  EXPECT_TRUE(
      CommandHasNoKeybinding(id, manifest_values::kPageActionCommandEvent));

  // Verify extension now has an MV3 action command of Alt+<keyboard_code>.
  EXPECT_TRUE(CommandHasAltPlusKeybinding(
      id, manifest_values::kActionCommandEvent, keyboard_code));
}

bool CommandServiceMv3UpgradeTest::CommandHasAltPlusKeybinding(
    const std::string& id,
    const char* action_command_name,
    const ui::KeyboardCode key) {
  ui::Accelerator accelerator = command_service()
                                    ->FindCommandByName(id, action_command_name)
                                    .accelerator();
  return key == accelerator.key_code() && accelerator.IsAltDown() &&
         !accelerator.IsShiftDown() && !accelerator.IsCtrlDown();
}
bool CommandServiceMv3UpgradeTest::CommandHasNoKeybinding(
    const std::string& id,
    const char* action_command_name) {
  ui::Accelerator accelerator = command_service()
                                    ->FindCommandByName(id, action_command_name)
                                    .accelerator();
  return ui::VKEY_UNKNOWN == accelerator.key_code();
}

// Tests that a suggested keybinding for an action command in MV2 persists when
// the extension is reloaded (unpacked) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       ActionCommandUpgradesDuringReload) {
  const std::string unpacked_extension_id = LoadExtensionMv2(
      /*suggested_keybinding=*/kAltNKeybinding);
  ReloadExtensionMv3(unpacked_extension_id,
                     /*new_suggested_keybinding=*/kAltNKeybinding);
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(unpacked_extension_id,
                                        /*keyboard_code=*/ui::VKEY_N);
}

// Tests that a suggested keybinding for an action command in MV2 persists when
// the extension is updated from the webstore and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       ActionCommandUpgradesDuringWebstoreUpdate) {
  InstallExtensionMv2(kExtensionId);
  UpdateExtensionMv3(kExtensionId,
                     /*mv3_extension_upgrade_folder=*/"v2_upgrade");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_N);
}

// Tests that a suggested keybinding for an action command in MV2 persists when
// the extension is updated (via .crx) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       ActionCommandUpgradesDuringCrxUpdate) {
  InstallExtensionMv2(kExtensionId, /*install_from_crx=*/true);
  UpdateExtensionMv3(kExtensionId,
                     /*mv3_extension_upgrade_folder=*/"v2_upgrade");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_N);
}

// Tests that a user modified keybinding for an action command in MV2 persists
// when the extension is reloaded (unpacked) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       UserModifiedActionCommandSurvivesReload) {
  const std::string unpacked_extension_id = LoadExtensionMv2(
      /*suggested_keybinding=*/kAltNKeybinding);
  ChangeMv2CommandKeybinding(unpacked_extension_id,
                             /*new_keybinding=*/kAltZKeybinding);
  ReloadExtensionMv3(unpacked_extension_id,
                     /*new_suggested_keybinding=*/kAltNKeybinding);
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(unpacked_extension_id,
                                        /*keyboard_code=*/ui::VKEY_Z);
}

// Tests that a user-modified keybinding for an action command in MV2 persists
// when the extension is updated from the webstore and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       UserModifiedCommandSurvivesWebstoreUpdate) {
  InstallExtensionMv2(kExtensionId);
  ChangeMv2CommandKeybinding(kExtensionId,
                             /*new_keybinding=*/kAltZKeybinding);
  UpdateExtensionMv3(kExtensionId,
                     /*mv3_extension_upgrade_folder=*/"v2_upgrade");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_Z);
}

// Tests that a user modified keybinding for an action command in MV2 persists
// when the extension is updated (via .crx) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       UserModifiedCommandSurvivesCrxUpdate) {
  InstallExtensionMv2(kExtensionId, /*install_from_crx=*/true);
  ChangeMv2CommandKeybinding(kExtensionId,
                             /*new_keybinding=*/kAltZKeybinding);
  UpdateExtensionMv3(kExtensionId,
                     /*mv3_extension_upgrade_folder=*/"v2_upgrade");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_Z);
}

// Tests that a modified suggested keybinding for an action command in MV2
// persists when the extension is reloaded (unpacked) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       SuggestedKeyModifiedActionCommandSurvivesReload) {
  const std::string unpacked_extension_id = LoadExtensionMv2(
      /*suggested_keybinding=*/kAltNKeybinding);
  ReloadExtensionMv3(unpacked_extension_id,
                     /*new_suggested_keybinding=*/"Alt+U");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(unpacked_extension_id,
                                        /*keyboard_code=*/ui::VKEY_U);
}

// Tests that a modified suggested keybinding for an action command in MV2
// persists when the extension is updated (via webstore) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       SuggestedKeyModifiedActionCommandSurvivesWebstore) {
  InstallExtensionMv2(kExtensionId);
  UpdateExtensionMv3(kExtensionId, "v2_upgrade_reassign");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_U);
}

// Tests that a modified suggested keybinding for an action command in MV2
// persists when the extension is updated (via .crx) and upgraded to MV3
// (_execute_action_command).
IN_PROC_BROWSER_TEST_P(CommandServiceMv3UpgradeTest,
                       SuggestedKeyModifiedActionCommandSurvivesCrx) {
  InstallExtensionMv2(kExtensionId, /*install_from_crx=*/true);
  UpdateExtensionMv3(kExtensionId,
                     /*mv3_extension_upgrade_folder=*/"v2_upgrade_reassign");
  ASSERT_FALSE(HasFatalFailure() && HasNonfatalFailure());

  CheckMv3UpgradeHasExpectedKeybindings(kExtensionId,
                                        /*keyboard_code=*/ui::VKEY_U);
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest, RemoveShortcutSurvivesUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v2"),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kExtensionId));

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator =
      command_service
          ->FindCommandByName(kExtensionId,
                              manifest_values::kBrowserActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Remove the keybinding.
  command_service->RemoveKeybindingPrefs(
      kExtensionId, manifest_values::kBrowserActionCommandEvent);

  // Verify it got removed.
  accelerator =
      command_service
          ->FindCommandByName(kExtensionId,
                              manifest_values::kBrowserActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2.
  EXPECT_TRUE(UpdateExtension(kExtensionId, path_v2, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kExtensionId));

  // Verify it is still set to nothing.
  accelerator =
      command_service
          ->FindCommandByName(kExtensionId,
                              manifest_values::kBrowserActionCommandEvent)
          .accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       RemoveKeybindingPrefsShouldBePlatformSpecific) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  ScopedDictPrefUpdate updater(browser()->profile()->GetPrefs(),
                               prefs::kExtensionCommands);
  base::Value::Dict& bindings = updater.Get();

  // Simulate command |toggle-feature| has been assigned with a shortcut on
  // another platform.
  std::string anotherPlatformKey = GetAnotherCommandPlatform() + ":Alt+G";
  const char kNamedCommandName[] = "toggle-feature";
  base::Value::Dict keybinding;
  keybinding.Set("extension", extension->id());
  keybinding.Set("command_name", kNamedCommandName);
  keybinding.Set("global", false);
  bindings.Set(anotherPlatformKey, std::move(keybinding));

  CommandService* command_service = CommandService::Get(browser()->profile());
  command_service->RemoveKeybindingPrefs(extension->id(), kNamedCommandName);

  // Removal of keybinding preference should be platform-specific, so the key on
  // another platform should always remained.
  EXPECT_TRUE(bindings.Find(anotherPlatformKey));
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       GetExtensionActionCommandQueryAll) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    Command command;
    bool active = false;
    EXPECT_TRUE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ALL,
        &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBasicAlternateKeybinding);

  {
    Command command;
    bool active = false;
    EXPECT_TRUE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ALL,
        &command, &active));

    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->RemoveKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent);

  {
    Command command;
    bool active = true;
    EXPECT_TRUE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ALL,
        &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_FALSE(active);
  }
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       GetExtensionActionCommandQueryActive) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    Command command;
    bool active = false;
    EXPECT_TRUE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ACTIVE,
        &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBasicAlternateKeybinding);

  {
    Command command;
    bool active = false;
    EXPECT_TRUE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ACTIVE,
        &command, &active));

    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->RemoveKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent);

  {
    Command command;
    bool active = false;
    EXPECT_FALSE(command_service->GetExtensionActionCommand(
        extension->id(), ActionInfo::Type::kBrowser, CommandService::ACTIVE,
        &command, &active));
  }
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       GetNamedCommandsQueryAll) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::ALL, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicNamedKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }

  command_service->UpdateKeybindingPrefs(
      extension->id(), kBasicNamedCommand, kBasicAlternateKeybinding);

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::ALL, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }

  command_service->RemoveKeybindingPrefs(extension->id(), kBasicNamedCommand);

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::ALL, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicNamedKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest, GetNamedCommandsQueryActive) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::ACTIVE, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicNamedKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }

  command_service->UpdateKeybindingPrefs(
      extension->id(), kBasicNamedCommand, kBasicAlternateKeybinding);

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::ACTIVE, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }

  command_service->RemoveKeybindingPrefs(extension->id(), kBasicNamedCommand);

  {
    CommandMap command_map;
    command_service->GetNamedCommands(
        extension->id(), CommandService::ACTIVE, CommandService::ANY_SCOPE,
        &command_map);
    EXPECT_EQ(0u, command_map.count(kBasicNamedCommand));
  }
}

// Test both browser and page actions for these tests.
INSTANTIATE_TEST_SUITE_P(BrowserAction,
                         CommandServiceMv3UpgradeTest,
                         testing::Values(ManifestCommandTestParameters{
                             manifest_keys::kBrowserAction,
                             manifest_values::kBrowserActionCommandEvent,
                             "v1_upgrade_browser_action"}));

INSTANTIATE_TEST_SUITE_P(PageAction,
                         CommandServiceMv3UpgradeTest,
                         testing::Values(ManifestCommandTestParameters{
                             manifest_keys::kPageAction,
                             manifest_values::kPageActionCommandEvent,
                             "v1_upgrade_page_action"}));

}  // namespace extensions
