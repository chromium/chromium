// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/bus.h"

class PrefService;

namespace keyboard {
class KeyboardUIFactory;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
}

namespace ash {

class ShellDelegate;

struct ASH_EXPORT ShellInitParams {
  ShellInitParams();
  ShellInitParams(ShellInitParams&& other);
  ~ShellInitParams();

  std::unique_ptr<ShellDelegate> delegate;
  ui::ContextFactory* context_factory = nullptr;                 // Non-owning.
  ui::ContextFactoryPrivate* context_factory_private = nullptr;  // Non-owning.
  PrefService* local_state = nullptr;                            // Non-owning.

  // Connector used by Shell to establish connections.
  service_manager::Connector* connector = nullptr;

  // Factory for creating the virtual keyboard UI. Must be non-null.
  std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory;

  // Bus used by dbus clients. May be null in tests or when not running on a
  // device, in which case fake clients will be created.
  scoped_refptr<dbus::Bus> dbus_bus;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
