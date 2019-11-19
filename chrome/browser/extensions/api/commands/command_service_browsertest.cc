// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_constants.h"

namespace {
const char kBasicBrowserActionKeybinding[] = "Ctrl+Shift+F";
const char kBasicNamedKeybinding[] = "Ctrl+Shift+Y";
const char kBasicAlternateKeybinding[] = "Ctrl+Shift+G";
const char kBasicNamedCommand[] = "toggle-feature";

// Get another command platform, whcih is used for simulating a command has been
// assigned with a shortcut on another platform.
std::string GetAnotherCommandPlatform() {
#if defined(OS_WIN)
  return extensions::manifest_values::kKeybindingPlatformMac;
#elif defined(OS_MACOSX)
  return extensions::manifest_values::kKeybindingPlatformChromeOs;
#elif defined(OS_CHROMEOS)
  return extensions::manifest_values::kKeybindingPlatformLinux;
#elif defined(OS_LINUX)
  return extensions::manifest_values::kKeybindingPlatformWin;
#else
  return "";
#endif
}

}  // namespace

namespace extensions {

typedef ExtensionApiTest CommandServiceTest;

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

  const char kId[] = "pgoakhfeplldmjheffidklpoklkppipp";

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED));

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Remove the keybinding.
  command_service->RemoveKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent);

  // Verify it got removed.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED));

  // Verify it is still set to nothing.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       RemoveKeybindingPrefsShouldBePlatformSpecific) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  DictionaryPrefUpdate updater(browser()->profile()->GetPrefs(),
                               prefs::kExtensionCommands);
  base::DictionaryValue* bindings = updater.Get();

  // Simulate command |toggle-feature| has been assigned with a shortcut on
  // another platform.
  std::string anotherPlatformKey = GetAnotherCommandPlatform() + ":Alt+G";
  const char kNamedCommandName[] = "toggle-feature";
  auto keybinding = std::make_unique<base::DictionaryValue>();
  keybinding->SetString("extension", extension->id());
  keybinding->SetString("command_name", kNamedCommandName);
  keybinding->SetBoolean("global", false);
  bindings->Set(anotherPlatformKey, std::move(keybinding));

  CommandService* command_service = CommandService::Get(browser()->profile());
  command_service->RemoveKeybindingPrefs(extension->id(), kNamedCommandName);

  // Removal of keybinding preference should be platform-specific, so the key on
  // another platform should always remained.
  EXPECT_TRUE(bindings->HasKey(anotherPlatformKey));
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
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ALL, &command, &active));

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
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ALL, &command, &active));

    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->RemoveKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent);

  {
    Command command;
    bool active = true;
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ALL, &command, &active));

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
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ACTIVE, &command, &active));

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
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ACTIVE, &command, &active));

    EXPECT_EQ(kBasicAlternateKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->RemoveKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent);

  {
    Command command;
    bool active = false;
    EXPECT_FALSE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::ACTIVE, &command, &active));
  }
}

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       GetExtensionActionCommandQuerySuggested) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    Command command;
    bool active = false;
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::SUGGESTED, &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_TRUE(active);
  }

  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBasicAlternateKeybinding);

  {
    Command command;
    bool active = true;
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::SUGGESTED, &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_FALSE(active);
  }

  command_service->RemoveKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent);

  {
    Command command;
    bool active = true;
    EXPECT_TRUE(command_service->GetBrowserActionCommand(
        extension->id(), CommandService::SUGGESTED, &command, &active));

    EXPECT_EQ(kBasicBrowserActionKeybinding,
              Command::AcceleratorToString(command.accelerator()));
    EXPECT_FALSE(active);
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

IN_PROC_BROWSER_TEST_F(CommandServiceTest,
                       GetNamedCommandsQuerySuggested) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("keybinding").AppendASCII("basics");
  const Extension* extension = InstallExtension(extension_dir, 1);
  ASSERT_TRUE(extension);

  CommandService* command_service = CommandService::Get(browser()->profile());

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::SUGGESTED, CommandService::ANY_SCOPE,
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
        extension->id(), CommandService::SUGGESTED, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicNamedKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }

  command_service->RemoveKeybindingPrefs(extension->id(), kBasicNamedCommand);

  {
    CommandMap command_map;
    EXPECT_TRUE(command_service->GetNamedCommands(
        extension->id(), CommandService::SUGGESTED, CommandService::ANY_SCOPE,
        &command_map));

    ASSERT_EQ(1u, command_map.count(kBasicNamedCommand));
    Command command = command_map[kBasicNamedCommand];
    EXPECT_EQ(kBasicNamedKeybinding,
              Command::AcceleratorToString(command.accelerator()));
  }
}

}  // namespace extensions
