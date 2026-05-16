// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr std::string_view kManifestWithoutCommands = R"(
            "manifest_version": 3,
            "version": "1")";
constexpr std::string_view kManifestWithRegularCommand = R"(
            "manifest_version": 3,
            "version": "1",
            "commands": {
              "regular_command": {
                "suggested_key": { "default": "Ctrl+Shift+2" },
                "description": "regular"
              }
            })";
constexpr std::string_view kManifestWithAlternateRegularCommand = R"(
            "manifest_version": 3,
            "version": "1",
            "commands": {
              "regular_command": {
                "suggested_key": { "default": "Ctrl+Shift+4" },
                "description": "regular"
              }
            })";
constexpr std::string_view kManifestWithUnsanitizedShortcuts = R"(
            "manifest_version": 3,
            "version": "1",
            "commands": {
              "legit_global_shortcut": {
                "suggested_key": { "default": "Ctrl+Shift+5" },
                "description": "legit global",
                "global": true
              },
              "legit_regular_shortcut": {
                "suggested_key": { "default": "Ctrl+Shift+6" },
                "description": "legit regular"
              },
              "hijack_ctrl_t": {
                "suggested_key": { "default": "Ctrl+T" },
                "description": "hijack",
                "global": true
              }
            })";

class FakeGlobalAcceleratorListener : public ui::GlobalAcceleratorListener {
 public:
  using ExecuteCommandCallback =
      base::RepeatingCallback<void(const std::string&, const std::string&)>;

  FakeGlobalAcceleratorListener() = default;
  FakeGlobalAcceleratorListener(const FakeGlobalAcceleratorListener&) = delete;
  FakeGlobalAcceleratorListener& operator=(
      const FakeGlobalAcceleratorListener&) = delete;
  ~FakeGlobalAcceleratorListener() override = default;

  // ui::GlobalAcceleratorListener:
  void StartListening() override {}

  void StopListening() override {}

  bool StartListeningForAccelerator(const ui::Accelerator&) override {
    return true;
  }

  void StopListeningForAccelerator(const ui::Accelerator&) override {}

  bool IsRegistrationHandledExternally() const override {
    return registration_handled_externally_;
  }

  void PruneStaleCommands() override {}

  void OnCommandsChanged(const std::string& extension_id,
                         const std::string& profile_id,
                         const ui::CommandMap& commands,
                         gfx::AcceleratedWidget widget,
                         ExecuteCommandCallback execute_command) override {
    last_execute_command_callback_ = std::move(execute_command);
    last_commands_ = commands;
  }

  bool has_last_execute_command_callback() const {
    return !last_execute_command_callback_.is_null();
  }

  const ExecuteCommandCallback& last_execute_command_callback() const {
    return last_execute_command_callback_;
  }

  const ui::CommandMap& last_commands() const { return last_commands_; }

  void set_registration_handled_externally(bool value) {
    registration_handled_externally_ = value;
  }

 private:
  bool registration_handled_externally_ = false;
  ExecuteCommandCallback last_execute_command_callback_;
  ui::CommandMap last_commands_;
};

class TestExtensionCommandsGlobalRegistry
    : public ExtensionCommandsGlobalRegistry {
 public:
  TestExtensionCommandsGlobalRegistry(
      content::BrowserContext* context,
      ui::GlobalAcceleratorListener* global_shortcut_listener)
      : ExtensionCommandsGlobalRegistry(context),
        global_shortcut_listener_(global_shortcut_listener) {}

 protected:
  // ExtensionCommandsGlobalRegistry:
  ui::GlobalAcceleratorListener* GetGlobalAcceleratorListener() const override {
    return global_shortcut_listener_;
  }

  bool RegisterAccelerator(const ui::Accelerator& accelerator,
                           const ExtensionId& extension_id,
                           const std::string& command_name) override {
    return true;
  }

 private:
  raw_ptr<ui::GlobalAcceleratorListener> global_shortcut_listener_;
};

class ExtensionCommandsGlobalRegistryTest : public ExtensionServiceTestBase {
 public:
  void InitializeExtensionService(ExtensionServiceInitParams params) override {
    params.testing_factories.emplace_back(
        ExtensionCommandsGlobalRegistry::GetFactoryInstance(),
        base::BindRepeating(
            [](content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
    ExtensionServiceTestBase::InitializeExtensionService(std::move(params));
  }

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    RecreateRegistry(/*global_shortcut_listener=*/&listener_);
  }

  void TearDown() override {
    ResetRegistry();
    ExtensionServiceTestBase::TearDown();
  }

 protected:
  void ResetRegistry() {
    listener_.SetShortcutHandlingSuspended(false);
    // Unregister the directly-owned test registry instance.
    if (registry_) {
      listener_.UnregisterAccelerators(registry_.get());
    }
    registry_.reset();
  }

  void RecreateRegistry(
      ui::GlobalAcceleratorListener* global_shortcut_listener) {
    if (registry_) {
      ResetRegistry();
    }
    registry_ = std::make_unique<TestExtensionCommandsGlobalRegistry>(
        profile(), global_shortcut_listener);
  }

  scoped_refptr<const Extension> BuildAndEnableExtension(
      std::string_view id,
      const std::string& json) {
    auto extension = ExtensionBuilder("commands")
                         .SetID(std::string(id))
                         .AddJSON(json)
                         .Build();
    auto* extension_registry = ExtensionRegistry::Get(profile());
    if (!extension_registry) {
      ADD_FAILURE() << "ExtensionRegistry is null";
      return nullptr;
    }
    extension_registry->AddEnabled(extension);
    return extension;
  }

  void UpdateKeybindings(const Extension* extension) {
    auto* service = CommandService::Get(profile());
    ASSERT_TRUE(service);
    service->UpdateKeybindingsForTest(extension);
  }

  void EnableExternalHandlingAndUpdate(const Extension* extension) {
    listener_.set_registration_handled_externally(true);
    UpdateKeybindings(extension);
  }

  FakeGlobalAcceleratorListener& listener() { return listener_; }

 private:
  FakeGlobalAcceleratorListener listener_;
  std::unique_ptr<TestExtensionCommandsGlobalRegistry> registry_;
};

// Tests PopulateCommands/destructor early-return behavior when listener is
// unavailable.
TEST_F(ExtensionCommandsGlobalRegistryTest,
       NullListenerPopulateAndDestructorAreSafe) {
  RecreateRegistry(/*global_shortcut_listener=*/nullptr);

  auto extension =
      BuildAndEnableExtension("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
                              std::string(kManifestWithRegularCommand));
  ASSERT_TRUE(extension);

  UpdateKeybindings(extension.get());
}

// Tests PopulateCommands callback is cancelled after registry destruction.
TEST_F(ExtensionCommandsGlobalRegistryTest,
       PopulateCommandsCallbackIsCancelledAfterRegistryDestruction) {
  auto extension = BuildAndEnableExtension(
      "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh",
      std::string(kManifestWithAlternateRegularCommand));
  ASSERT_TRUE(extension);

  EnableExternalHandlingAndUpdate(extension.get());

  ASSERT_TRUE(listener().has_last_execute_command_callback());
  auto execute_command = listener().last_execute_command_callback();
  EXPECT_FALSE(execute_command.IsCancelled());

  ResetRegistry();
  EXPECT_TRUE(execute_command.IsCancelled());
}

// Tests externally handled PopulateCommands returns false when no named
// commands exist.
TEST_F(ExtensionCommandsGlobalRegistryTest,
       PopulateCommandsExternallyHandledWithoutNamedCommandsReturnsFalse) {
  auto extension =
      BuildAndEnableExtension("jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj",
                              std::string(kManifestWithoutCommands));
  ASSERT_TRUE(extension);

  EnableExternalHandlingAndUpdate(extension.get());

  EXPECT_FALSE(listener().has_last_execute_command_callback());
}

// Tests that PopulateCommands correctly sanitizes reserved shortcuts and
// regular shortcuts but still forwards all commands to the portal list
// when registration is handled externally.
TEST_F(ExtensionCommandsGlobalRegistryTest,
       PopulateCommandsExternallyHandledSanitizesShortcuts) {
  auto extension =
      BuildAndEnableExtension("abcdefghijklmnopabcdefghijklmnop",
                              std::string(kManifestWithUnsanitizedShortcuts));
  ASSERT_TRUE(extension);

  EnableExternalHandlingAndUpdate(extension.get());

  ASSERT_TRUE(listener().has_last_execute_command_callback());
  const ui::CommandMap& commands = listener().last_commands();

  // "legit_global_shortcut" should be present with its accelerator because
  // it's global and not reserved.
  auto it_legit_global = commands.find("legit_global_shortcut");
  ASSERT_NE(it_legit_global, commands.end());
  EXPECT_EQ(ui::VKEY_5, it_legit_global->second.accelerator().key_code());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(it_legit_global->second.accelerator().IsCmdDown());
#else
  EXPECT_TRUE(it_legit_global->second.accelerator().IsCtrlDown());
#endif
  EXPECT_TRUE(it_legit_global->second.accelerator().IsShiftDown());

  // "legit_regular_shortcut" should be present (for manual assignment) but its
  // accelerator must be cleared because it's not a global shortcut and
  // should not be suggested to the global portal.
  auto it_legit_regular = commands.find("legit_regular_shortcut");
  ASSERT_NE(it_legit_regular, commands.end());
  EXPECT_EQ(ui::VKEY_UNKNOWN,
            it_legit_regular->second.accelerator().key_code());

  // "hijack_ctrl_t" should be present (for manual assignment) but its
  // accelerator must be cleared because it's a reserved shortcut.
  auto it_hijack = commands.find("hijack_ctrl_t");
  ASSERT_NE(it_hijack, commands.end());
  EXPECT_EQ(ui::VKEY_UNKNOWN, it_hijack->second.accelerator().key_code());
}

}  // namespace

}  // namespace extensions
