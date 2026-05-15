// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_LOCAL_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_GLIC_COMMON_LOCAL_HOTKEY_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_close_options.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

namespace gfx {
class Point;
}

#if !BUILDFLAG(IS_ANDROID)
namespace views {
class View;
}
#endif

namespace glic {

// Manages hotkeys that are active within a specific local scope, such as the
// Glic window itself or the broader Chrome application when Glic is relevant.
// This class handles retrieving accelerator definitions (potentially from user
// preferences), registering them within the appropriate scope via a Delegate,
// and dispatching pressed accelerators back to the Delegate for handling.
class LocalHotkeyManager : public ui::AcceleratorTarget {
 public:
  // Enum representing the different commands managed by this class.
  enum class Command {
    // Close the Glic window. Only works when the Glic window has focus.
    kClose,
    // Toggle focus between the Glic window and the last active
    // browser window.
    kFocusToggle,
    // Zoom in the web client.
    kZoomIn,
    // Zoom out the web client.
    kZoomOut,
    // Reset zoom level of the web client.
    kZoomReset,
#if BUILDFLAG(IS_WIN)
    // Show the title bar context menu
    kTitleBarContextMenu,
#endif
  };

  class Panel {
   public:
    virtual ~Panel() = default;
    virtual void FocusIfOpen() = 0;
    virtual bool HasFocus() = 0;
    virtual bool IsShowing() const = 0;
    virtual void Close(const CloseOptions& options) = 0;
    virtual bool ActivateBrowser() = 0;
    virtual void Zoom(mojom::ZoomAction action) = 0;
    virtual void ShowTitleBarContextMenuAt(gfx::Point event_loc) = 0;
#if !BUILDFLAG(IS_ANDROID)
    virtual bool HasSelectionOverlay() = 0;
    virtual void CloseSelectionOverlay() = 0;
    virtual base::WeakPtr<views::View> GetView() = 0;
#endif
  };

  constexpr static const char* CommandToString(Command command) {
    switch (command) {
      case Command::kClose:
        return "kClose";
      case Command::kFocusToggle:
        return "kFocusToggle";
      case Command::kZoomIn:
        return "kZoomIn";
      case Command::kZoomOut:
        return "kZoomOut";
      case Command::kZoomReset:
        return "kZoomReset";
#if BUILDFLAG(IS_WIN)
      case Command::kTitleBarContextMenu:
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

  // Delegate interface responsible for the actual registration of hotkeys
  // within a specific scope (e.g., Glic window, application-wide).
  class RegistrationDelegate {
   public:
    virtual ~RegistrationDelegate() = default;

    // Creates a ScopedHotkeyRegistration for the given accelerator.
    // The implementation should register the hotkey within its specific scope
    // and return an object that unregisters it upon destruction.
    virtual std::unique_ptr<ScopedHotkeyRegistration>
    CreateScopedHotkeyRegistration(
        ui::Accelerator accelerator,
        base::WeakPtr<ui::AcceleratorTarget> target) = 0;
  };

  // Interface for handling hotkey events.
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    // Called when a registered command associated with this manager is pressed.
    // Returns true if the accelerator was handled, false otherwise.
    virtual bool AcceleratorPressed(Command command) = 0;

    // Returns true if hotkeys can be handled right now.
    virtual bool CanHandleAccelerators() const = 0;
  };

  explicit LocalHotkeyManager(
      std::unique_ptr<RegistrationDelegate> registration_delegate,
      EventHandler* event_handler,
      base::span<const Command> supported_commands);
  ~LocalHotkeyManager() override;

  // Returns the default accelerator for a given command.
  static ui::Accelerator GetDefaultAccelerator(Command command);

  // Returns the hardcoded, non-configurable accelerators for a given command.
  // CHECKs if the command is not defined as static (i.e., not in
  // kCommandToStaticAcceleratorsMap).
  static base::span<const ui::Accelerator> GetStaticAccelerators(
      LocalHotkeyManager::Command command);

  // Returns the current configurable accelerator for a given command,
  // potentially reading from user preferences. Falls back to the default if
  // no preference is set or the preference is invalid. CHECKs if the passed
  // command is not defined as configurable.
  static ui::Accelerator GetConfigurableAccelerator(Command command);

  // Returns the Command enum value corresponding to the given accelerator.
  // CHECKs if the accelerator is not supported by this manager.
  Command GetCommand(ui::Accelerator accelerator);

  void InitializeAccelerators();

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  base::WeakPtr<LocalHotkeyManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<ui::Accelerator> GetAccelerators(Command command);
  void RegisterCommand(Command command);

  std::unique_ptr<RegistrationDelegate> registration_delegate_;
  raw_ptr<EventHandler> event_handler_;
  const std::vector<Command> supported_commands_;

  PrefChangeRegistrar pref_registrar_;
  base::flat_map<Command,
                 std::vector<std::unique_ptr<ScopedHotkeyRegistration>>>
      command_registrations_;
  base::WeakPtrFactory<LocalHotkeyManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_LOCAL_HOTKEY_MANAGER_H_
