// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_ASH_DBUS_SERVICES_H_
#define ASH_DBUS_ASH_DBUS_SERVICES_H_

#include <memory>

#include "base/macros.h"

namespace chromeos {
class CrosDBusService;
}

namespace dbus {
class Bus;
}

namespace ash {

// Owns and manages the lifetime of the ash D-Bus services.
class AshDBusServices {
 public:
  explicit AshDBusServices(dbus::Bus* system_bus);
  ~AshDBusServices();

 private:
  std::unique_ptr<chromeos::CrosDBusService> display_service_;
  std::unique_ptr<chromeos::CrosDBusService> gesture_properties_service_;
  std::unique_ptr<chromeos::CrosDBusService> liveness_service_;
  std::unique_ptr<chromeos::CrosDBusService> url_handler_service_;

  DISALLOW_COPY_AND_ASSIGN(AshDBusServices);
};

}  // namespace ash

#endif  // ASH_DBUS_ASH_DBUS_SERVICES_H_
