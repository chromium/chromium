// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/common/local_hotkey_manager.h"

#include <algorithm>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/accelerators/command.h"

namespace glic {

namespace {

#if BUILDFLAG(IS_MAC)
constexpr int kFocusToggleAcceleratorModifiers =
    ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN;
#elif BUILDFLAG(IS_CHROMEOS)
// ui::EF_COMMAND_DOWN is the search key for ChromeOS.
constexpr int kFocusToggleAcceleratorModifiers =
    ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN;
#else
constexpr int kFocusToggleAcceleratorModifiers =
    ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;
#endif

constexpr auto kCommandToPrefMap =
    base::MakeFixedFlatMap<LocalHotkeyManager::Command, const char*>(
        {{LocalHotkeyManager::Command::kFocusToggle,
          prefs::kGlicFocusToggleHotkey}});

constexpr std::array kCloseAccelerators = {
    ui::Accelerator{ui::VKEY_ESCAPE, ui::EF_NONE},
#if BUILDFLAG(IS_MAC)
    ui::Accelerator{ui::VKEY_W, ui::EF_COMMAND_DOWN}
#else
    ui::Accelerator{ui::VKEY_W, ui::EF_CONTROL_DOWN}
#endif
};

#if BUILDFLAG(IS_WIN)
constexpr std::array kTitleBarContextMenuAccelerators = {
    ui::Accelerator{ui::VKEY_SPACE, ui::EF_ALT_DOWN}};
#endif

#if BUILDFLAG(IS_MAC)
constexpr int kZoomModifier = ui::EF_COMMAND_DOWN;
#else
constexpr int kZoomModifier = ui::EF_CONTROL_DOWN;
#endif

constexpr std::array kZoomInAccelerators = {
    ui::Accelerator{ui::VKEY_OEM_PLUS, kZoomModifier},
    ui::Accelerator{ui::VKEY_ADD, kZoomModifier}};

constexpr std::array kZoomOutAccelerators = {
    ui::Accelerator{ui::VKEY_OEM_MINUS, kZoomModifier},
    ui::Accelerator{ui::VKEY_SUBTRACT, kZoomModifier}};

constexpr std::array kZoomResetAccelerators = {
    ui::Accelerator{ui::VKEY_0, kZoomModifier},
    ui::Accelerator{ui::VKEY_NUMPAD0, kZoomModifier}};

constexpr auto kCommandToStaticAcceleratorsMap =
    base::MakeFixedFlatMap<LocalHotkeyManager::Command,
                           base::span<const ui::Accelerator>>(
        {{LocalHotkeyManager::Command::kClose, kCloseAccelerators},
         {LocalHotkeyManager::Command::kZoomIn, kZoomInAccelerators},
         {LocalHotkeyManager::Command::kZoomOut, kZoomOutAccelerators},
         {LocalHotkeyManager::Command::kZoomReset, kZoomResetAccelerators},
#if BUILDFLAG(IS_WIN)
         {LocalHotkeyManager::Command::kTitleBarContextMenu,
          kTitleBarContextMenuAccelerators}
#endif
        });

// Compile-time helper to check if the keys in two maps are disjoint.
// It checks if any key from map1 exists in map2.
template <typename Map1, typename Map2>
constexpr bool AreKeysDisjoint(const Map1& map1, const Map2& map2) {
  for (const auto& entry_map1 : map1) {
    const auto& key_to_find = entry_map1.first;
    auto it = std::lower_bound(map2.begin(), map2.end(), key_to_find,
                               [](const auto& element, const auto& key) {
                                 return element.first < key;
                               });
    if (it != map2.end() && it->first == key_to_find) {
      return false;  // Key from map1 found in map2, so not disjoint.
    }
  }
  return true;
}
// Compile-time check to ensure that commands defined as static (in
// kCommandToStaticAcceleratorsMap) are not also defined as configurable (in
// kCommandToPrefMap).
static_assert(
    AreKeysDisjoint(kCommandToStaticAcceleratorsMap, kCommandToPrefMap),
    "A command cannot be defined in both kCommandToStaticAcceleratorsMap and "
    "kCommandToPrefMap. Check definitions.");
}  // namespace

LocalHotkeyManager::LocalHotkeyManager(
    std::unique_ptr<RegistrationDelegate> registration_delegate,
    EventHandler* event_handler,
    base::span<const Command> supported_commands)
    : registration_delegate_(std::move(registration_delegate)),
      event_handler_(event_handler),
      supported_commands_(supported_commands.begin(),
                          supported_commands.end()) {
  CHECK(registration_delegate_);
  CHECK(event_handler_);
  CHECK(g_browser_process);
  pref_registrar_.Init(g_browser_process->local_state());
  for (Command command : supported_commands_) {
    auto pref_name_iter = kCommandToPrefMap.find(command);
    if (pref_name_iter == kCommandToPrefMap.end()) {
      continue;
    }
    pref_registrar_.Add(
        pref_name_iter->second,
        base::BindRepeating(&LocalHotkeyManager::RegisterCommand,
                            base::Unretained(this), command));
  }
}

LocalHotkeyManager::~LocalHotkeyManager() {
  // Ensure that registrations are deleted before the WeakPtrs are invalidated
  // as they might be depending on it.
  command_registrations_.clear();
}

// static
ui::Accelerator LocalHotkeyManager::GetDefaultAccelerator(Command command) {
  CHECK(kCommandToPrefMap.contains(command));
  switch (command) {
    case Command::kFocusToggle:
      return ui::Accelerator{ui::VKEY_G, kFocusToggleAcceleratorModifiers};
    default:
      NOTREACHED();
  }
}

// static
base::span<const ui::Accelerator> LocalHotkeyManager::GetStaticAccelerators(
    Command command) {
  auto iter = kCommandToStaticAcceleratorsMap.find(command);
  CHECK(iter != kCommandToStaticAcceleratorsMap.end());
  return iter->second;
}

// static
ui::Accelerator LocalHotkeyManager::GetConfigurableAccelerator(
    Command command) {
  auto pref_name_iter = kCommandToPrefMap.find(command);
  CHECK(pref_name_iter != kCommandToPrefMap.end());

  // NEEDS_ANDROID_IMPL: StringToAccelerator does not work on Android.
  const ui::Accelerator accelerator = ui::Command::StringToAccelerator(
      g_browser_process->local_state()->GetString(pref_name_iter->second));

  // Return empty accelerator if an invalid modifier was set.
  if (!accelerator.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return accelerator;
}

LocalHotkeyManager::Command LocalHotkeyManager::GetCommand(
    ui::Accelerator accelerator) {
  CHECK(event_handler_);

  for (Command command : supported_commands_) {
    if (auto iter = kCommandToStaticAcceleratorsMap.find(command);
        iter != kCommandToStaticAcceleratorsMap.end()) {
      for (const ui::Accelerator& static_accelerator : iter->second) {
        if (static_accelerator == accelerator) {
          return command;
        }
      }
    } else if (GetConfigurableAccelerator(command) == accelerator) {
      return command;
    }
  }

  NOTREACHED() << accelerator.GetShortcutText() << " isn't a supported command";
}

void LocalHotkeyManager::InitializeAccelerators() {
  CHECK(event_handler_);
  for (Command command : supported_commands_) {
    RegisterCommand(command);
  }
}

bool LocalHotkeyManager::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!event_handler_) {
    return false;
  }
  auto command = GetCommand(accelerator);
  return event_handler_->AcceleratorPressed(command);
}

bool LocalHotkeyManager::CanHandleAccelerators() const {
  if (!event_handler_) {
    return false;
  }
  return event_handler_->CanHandleAccelerators();
}

std::vector<ui::Accelerator> LocalHotkeyManager::GetAccelerators(
    Command command) {
  std::vector<ui::Accelerator> accelerators_to_register;
  if (kCommandToPrefMap.contains(command)) {
    // Configurable hotkey
    ui::Accelerator accelerator = GetConfigurableAccelerator(command);
    if (!accelerator.IsEmpty()) {
      accelerators_to_register.push_back(accelerator);
    }
  } else {
    // Static hotkey. GetStaticAccelerators will CHECK that it's in
    // kCommandToStaticAcceleratorsMap and not in kCommandToPrefMap.
    base::span<const ui::Accelerator> static_accelerators =
        GetStaticAccelerators(command);
    accelerators_to_register.assign(static_accelerators.begin(),
                                    static_accelerators.end());
  }
  return accelerators_to_register;
}

void LocalHotkeyManager::RegisterCommand(Command command) {
  // Clear previous registrations for this command.
  // The unique_ptrs in the vector will be destroyed, unregistering them.
  command_registrations_.erase(command);

  if (!registration_delegate_) {
    return;
  }

  std::vector<std::unique_ptr<ScopedHotkeyRegistration>> new_registrations;

  for (const ui::Accelerator& accelerator : GetAccelerators(command)) {
    if (auto registration =
            registration_delegate_->CreateScopedHotkeyRegistration(
                accelerator, GetWeakPtr())) {
      new_registrations.push_back(std::move(registration));
    }
  }

  if (!new_registrations.empty()) {
    command_registrations_.emplace(command, std::move(new_registrations));
  }
}

}  // namespace glic
