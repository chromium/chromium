// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/local_hotkey_manager.h"

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

ui::Accelerator LocalHotkeyManager::GetDefaultAccelerator(Hotkey hotkey) {
  switch (hotkey) {
    case Hotkey::kFocusToggle:
      return ui::Accelerator{ui::VKEY_G, kFocusToggleAcceleratorModifiers};
    case Hotkey::kClose:
      return ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
#if BUILDFLAG(IS_WIN)
    case Hotkey::kTitleBarContextMenu:
      return ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
#endif
    default:
      NOTREACHED();
  }
}

ui::Accelerator LocalHotkeyManager::GetAccelerator(Hotkey hotkey) {
  auto pref_name_iter = kHotkeyToPrefMap.find(hotkey);
  if (pref_name_iter == kHotkeyToPrefMap.end()) {
    return GetDefaultAccelerator(hotkey);
  }

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
    if (GetAccelerator(hotkey) == accelerator) {
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

void LocalHotkeyManager::RegisterHotkey(Hotkey hotkey) {
  auto accelerator = GetAccelerator(hotkey);

  hotkey_registrations_.erase(hotkey);
  if (accelerator.IsEmpty()) {
    return;
  }

  if (!delegate_) {
    return;
  }

  if (auto registration = delegate_->CreateScopedHotkeyRegistration(
          accelerator, GetWeakPtr())) {
    hotkey_registrations_.emplace(hotkey, std::move(registration));
  }
}

}  // namespace glic
