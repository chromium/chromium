// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace ui {
class Accelerator;
}

namespace extensions {

// Extensions-specific wrapper on the general purpose
// `ui::GlobalAcceleratorListener`. Most calls are delegated to the general
// purpose service, but there is also a special Linux Dbus implementation that
// applies only to extensions.
class GlobalShortcutListener {
 public:
  class Observer : public ui::GlobalAcceleratorListener::Observer {
   public:
    // Called when a command should be executed directly.
    virtual void ExecuteCommand(const std::string& accelerator_group_id,
                                const std::string& command_id) = 0;
  };

  GlobalShortcutListener(const GlobalShortcutListener&) = delete;
  GlobalShortcutListener& operator=(const GlobalShortcutListener&) = delete;

  virtual ~GlobalShortcutListener();

  // The instance may be nullptr.
  static GlobalShortcutListener* GetInstance();

  // Register an observer for when a certain |accelerator| is struck. Returns
  // true if register successfully, or false if 1) the specificied |accelerator|
  // has been registered by another caller or other native applications, or
  // 2) shortcut handling is suspended.
  //
  // Note that we do not support recognizing that an accelerator has been
  // registered by another application on all platforms. This is a per-platform
  // consideration.
  bool RegisterAccelerator(const ui::Accelerator& accelerator,
                           Observer* observer);

  // Stop listening for the given |accelerator|, does nothing if shortcut
  // handling is suspended.
  void UnregisterAccelerator(const ui::Accelerator& accelerator,
                             Observer* observer);

  // Stop listening for all accelerators of the given |observer|, does nothing
  // if shortcut handling is suspended.
  virtual void UnregisterAccelerators(Observer* observer);

  // Suspend/Resume global shortcut handling. Note that when suspending,
  // RegisterAccelerator/UnregisterAccelerator/UnregisterAccelerators are not
  // allowed to be called until shortcut handling has been resumed.
  void SetShortcutHandlingSuspended(bool suspended);

  // Returns whether shortcut handling is currently suspended.
  bool IsShortcutHandlingSuspended() const;

  // Returns true if shortcut registration is managed by the desktop. False
  // indicates registration is managed by us.
  virtual bool IsRegistrationHandledExternally() const;

  // Called when an extension's commands are registered.
  virtual void OnCommandsChanged(const std::string& accelerator_group_id,
                                 const std::string& profile_id,
                                 const ui::CommandMap& commands,
                                 Observer* observer);

 protected:
  explicit GlobalShortcutListener(
      ui::GlobalAcceleratorListener* global_shortcut_listener);

 private:
  // Removes an accelerator from the list of accelerators registered with this
  // class.
  void RemoveAccelerator(const ui::Accelerator& accelerator);

  // GlobalShortcutListener instance to which where most calls are delegated.
  raw_ptr<ui::GlobalAcceleratorListener> global_accelerator_listener_;

  // Local tracking of accelerators that need to be registered or unregistered
  // when shortcut handling is suspended.
  std::vector<ui::Accelerator> accelerators_;

  // Keeps track of whether shortcut handling is currently suspended.
  bool shortcut_handling_suspended_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_
