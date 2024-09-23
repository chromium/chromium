// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/bus.h"

class PrefService;

namespace display {
class NativeDisplayDelegate;
}  // namespace display

namespace keyboard {
class KeyboardUIFactory;
}

namespace ui {
class ContextFactory;
}

namespace ash {

class ShellDelegate;

struct ASH_EXPORT ShellInitParams {
  ShellInitParams();
  ShellInitParams(ShellInitParams&& other);
  ~ShellInitParams();

  std::unique_ptr<ShellDelegate> delegate;
  raw_ptr<ui::ContextFactory> context_factory = nullptr;  // Non-owning.
  raw_ptr<PrefService> local_state = nullptr;             // Non-owning.

  // Factory for creating the virtual keyboard UI. Must be non-null.
  std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory;

  // Factory for creating the quick_pair mediator.
  std::unique_ptr<ash::quick_pair::Mediator::Factory>
      quick_pair_mediator_factory;

  // Bus used by dbus clients. May be null in tests or when not running on a
  // device, in which case fake clients will be created.
  scoped_refptr<dbus::Bus> dbus_bus;

  // A native display delegate used in the shell.
  std::unique_ptr<display::NativeDisplayDelegate> native_display_delegate;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
