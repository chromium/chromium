// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/widget/local_hotkey_manager.h"

#include <algorithm>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/accelerators/command.h"

namespace glic {

namespace {

#if BUILDFLAG(IS_MAC)
constexpr int kFocusToggleAcceleratorModifiers =
    ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN;
#else
constexpr int kFocusToggleAcceleratorModifiers =
    ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;
#endif

constexpr auto kHotkeyToPrefMap =
    base::MakeFixedFlatMap<LocalHotkeyManager::Hotkey, const char*>(
        {{LocalHotkeyManager::Hotkey::kFocusToggle,
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

constexpr auto kHotkeyToStaticAcceleratorsMap =
    base::MakeFixedFlatMap<LocalHotkeyManager::Hotkey,
                           base::span<const ui::Accelerator>>(
        {{LocalHotkeyManager::Hotkey::kClose, kCloseAccelerators},
#if BUILDFLAG(IS_WIN)
         {LocalHotkeyManager::Hotkey::kTitleBarContextMenu,
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
// Compile-time check to ensure that hotkeys defined as static (in
// kHotkeyToStaticAcceleratorsMap) are not also defined as configurable (in
// kHotkeyToPrefMap).
static_assert(
    AreKeysDisjoint(kHotkeyToStaticAcceleratorsMap, kHotkeyToPrefMap),
    "A hotkey cannot be defined in both kHotkeyToStaticAcceleratorsMap and "
    "kHotkeyToPrefMap. Check definitions.");
}  // namespace

LocalHotkeyManager::LocalHotkeyManager(
    base::WeakPtr<GlicWindowController> window_controller,
    std::unique_ptr<Delegate> delegate)
    : window_controller_(window_controller), delegate_(std::move(delegate)) {
  CHECK(delegate_);
  CHECK(g_browser_process);
  pref_registrar_.Init(g_browser_process->local_state());
  for (Hotkey hotkey : delegate_->GetSupportedHotkeys()) {
    auto pref_name_iter = kHotkeyToPrefMap.find(hotkey);
    if (pref_name_iter == kHotkeyToPrefMap.end()) {
      continue;
    }
    pref_registrar_.Add(pref_name_iter->second,
                        base::BindRepeating(&LocalHotkeyManager::RegisterHotkey,
                                            base::Unretained(this), hotkey));
  }
}

LocalHotkeyManager::~LocalHotkeyManager() {
  // Ensure that registrations are deleted before the WeakPtrs are invalidated
  // as they might be depending on it.
  hotkey_registrations_.clear();
}

// static
ui::Accelerator LocalHotkeyManager::GetDefaultAccelerator(Hotkey hotkey) {
  CHECK(kHotkeyToPrefMap.contains(hotkey));
  switch (hotkey) {
    case Hotkey::kFocusToggle:
      return ui::Accelerator{ui::VKEY_G, kFocusToggleAcceleratorModifiers};
    default:
      NOTREACHED();
  }
}

// static
base::span<const ui::Accelerator> LocalHotkeyManager::GetStaticAccelerators(
    Hotkey hotkey) {
  auto iter = kHotkeyToStaticAcceleratorsMap.find(hotkey);
  CHECK(iter != kHotkeyToStaticAcceleratorsMap.end());
  return iter->second;
}

// static
ui::Accelerator LocalHotkeyManager::GetConfigurableAccelerator(Hotkey hotkey) {
  auto pref_name_iter = kHotkeyToPrefMap.find(hotkey);
  CHECK(pref_name_iter != kHotkeyToPrefMap.end());

  const ui::Accelerator accelerator = ui::Command::StringToAccelerator(
      g_browser_process->local_state()->GetString(pref_name_iter->second));

  // Return empty accelerator if an invalid modifier was set.
  if (!accelerator.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return accelerator;
}

LocalHotkeyManager::Hotkey LocalHotkeyManager::GetHotkeyEnum(
    ui::Accelerator accelerator) {
  CHECK(delegate_);

  for (Hotkey hotkey : delegate_->GetSupportedHotkeys()) {
    if (auto iter = kHotkeyToStaticAcceleratorsMap.find(hotkey);
        iter != kHotkeyToStaticAcceleratorsMap.end()) {
      for (const ui::Accelerator& static_accelerator : iter->second) {
        if (static_accelerator == accelerator) {
          return hotkey;
        }
      }
    } else if (GetConfigurableAccelerator(hotkey) == accelerator) {
      return hotkey;
    }
  }

  NOTREACHED() << accelerator.GetShortcutText() << " isn't a supported hotkey";
}

void LocalHotkeyManager::InitializeAccelerators() {
  CHECK(delegate_);
  for (Hotkey hotkey : delegate_->GetSupportedHotkeys()) {
    RegisterHotkey(hotkey);
  }
}

bool LocalHotkeyManager::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!window_controller_ || !delegate_) {
    return false;
  }
  auto hotkey = GetHotkeyEnum(accelerator);
  return delegate_->AcceleratorPressed(hotkey);
}

bool LocalHotkeyManager::CanHandleAccelerators() const {
  if (!window_controller_) {
    return false;
  }

  return window_controller_->IsShowing();
}

std::vector<ui::Accelerator> LocalHotkeyManager::GetAccelerators(
    Hotkey hotkey) {
  std::vector<ui::Accelerator> accelerators_to_register;
  if (kHotkeyToPrefMap.contains(hotkey)) {
    // Configurable hotkey
    ui::Accelerator accelerator = GetConfigurableAccelerator(hotkey);
    if (!accelerator.IsEmpty()) {
      accelerators_to_register.push_back(accelerator);
    }
  } else {
    // Static hotkey. GetStaticAccelerators will CHECK that it's in
    // kHotkeyToStaticAcceleratorsMap and not in kHotkeyToPrefMap.
    base::span<const ui::Accelerator> static_accelerators =
        GetStaticAccelerators(hotkey);
    accelerators_to_register.assign(static_accelerators.begin(),
                                    static_accelerators.end());
  }
  return accelerators_to_register;
}

void LocalHotkeyManager::RegisterHotkey(Hotkey hotkey) {
  // Clear previous registrations for this hotkey.
  // The unique_ptrs in the vector will be destroyed, unregistering them.
  hotkey_registrations_.erase(hotkey);

  if (!delegate_) {
    return;
  }

  std::vector<std::unique_ptr<ScopedHotkeyRegistration>> new_registrations;

  for (const ui::Accelerator& accelerator : GetAccelerators(hotkey)) {
    if (auto registration = delegate_->CreateScopedHotkeyRegistration(
            accelerator, GetWeakPtr())) {
      new_registrations.push_back(std::move(registration));
    }
  }

  if (!new_registrations.empty()) {
    hotkey_registrations_.emplace(hotkey, std::move(new_registrations));
  }
}

}  // namespace glic
