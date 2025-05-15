// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_LOCAL_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_GLIC_WIDGET_LOCAL_HOTKEY_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {
class GlicWindowController;

// Manages hotkeys that are active within a specific local scope, such as the
// Glic window itself or the broader Chrome application when Glic is relevant.
// This class handles retrieving accelerator definitions (potentially from user
// preferences), registering them within the appropriate scope via a Delegate,
// and dispatching pressed accelerators back to the Delegate for handling.
class LocalHotkeyManager : public ui::AcceleratorTarget {
 public:
  // Enum representing the different hotkeys managed by this class.
  enum class Hotkey {
    // Close the Glic window. Only works when the Glic window has focus.
    kClose,
    // Toggle focus between the Glic window and the last active
    // browser window.
    kFocusToggle,
#if BUILDFLAG(IS_WIN)
    // Show the title bar context menu
    kTitleBarContextMenu,
#endif
  };

  constexpr static const char* HotkeyToString(Hotkey hotkey) {
    switch (hotkey) {
      case Hotkey::kClose:
        return "kClose";
      case Hotkey::kFocusToggle:
        return "kFocusToggle";
#if BUILDFLAG(IS_WIN)
      case Hotkey::kTitleBarContextMenu:
        return "kTitleBarContextMenu";
#endif
    }
  }

  // Interface for managing the lifetime of a registered hotkey.
  // Implementations handle the specific registration/unregistration logic
  // for their scope (e.g., adding/removing from a views::View or
  // views::FocusManager).
  class ScopedHotkeyRegistration {
   public:
    virtual ~ScopedHotkeyRegistration() = default;
  };

  // Delegate interface responsible for the actual registration and handling
  // of hotkeys within a specific scope (e.g., Glic window, application-wide).
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual const base::span<const Hotkey> GetSupportedHotkeys() const = 0;

    // Creates a ScopedHotkeyRegistration for the given accelerator.
    // The implementation should register the hotkey within its specific scope
    // and return an object that unregisters it upon destruction.
    virtual std::unique_ptr<ScopedHotkeyRegistration>
    CreateScopedHotkeyRegistration(
        ui::Accelerator accelerator,
        base::WeakPtr<ui::AcceleratorTarget> target) = 0;

    // Called when a registered hotkey associated with this manager is pressed.
    // Returns true if the accelerator was handled, false otherwise.
    virtual bool AcceleratorPressed(Hotkey) = 0;
  };

  explicit LocalHotkeyManager(
      base::WeakPtr<GlicWindowController> window_controller,
      std::unique_ptr<Delegate> delegate);
  ~LocalHotkeyManager() override;

  // Returns the default accelerator for a given hotkey.
  static ui::Accelerator GetDefaultAccelerator(Hotkey hotkey_enum);

  // Returns the hardcoded, non-configurable accelerators for a given hotkey.
  // CHECKs if the hotkey is not defined as static (i.e., not in
  // kHotkeyToStaticAcceleratorsMap).
  static base::span<const ui::Accelerator> GetStaticAccelerators(
      LocalHotkeyManager::Hotkey hotkey);

  // Returns the current configurable accelerator for a given hotkey,
  // potentially reading from user preferences. Falls back to the default if
  // no preference is set or the preference is invalid. CHECKs if the passed
  // hotkey_enum is not defined as configurable.
  static ui::Accelerator GetConfigurableAccelerator(Hotkey hotkey_enum);

  // Returns the Hotkey enum value corresponding to the given accelerator.
  // CHECKs if the accelerator is not supported by this manager.
  Hotkey GetHotkeyEnum(ui::Accelerator accelerator);

  void InitializeAccelerators();

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  base::WeakPtr<LocalHotkeyManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<ui::Accelerator> GetAccelerators(Hotkey hotkey);
  void RegisterHotkey(Hotkey hotkey_enum);

  base::WeakPtr<GlicWindowController> window_controller_;
  std::unique_ptr<Delegate> delegate_;

  PrefChangeRegistrar pref_registrar_;
  base::flat_map<Hotkey, std::vector<std::unique_ptr<ScopedHotkeyRegistration>>>
      hotkey_registrations_;
  base::WeakPtrFactory<LocalHotkeyManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_LOCAL_HOTKEY_MANAGER_H_
