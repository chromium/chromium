// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_ASH_DBUS_SERVICES_H_
#define ASH_DBUS_ASH_DBUS_SERVICES_H_

#include <memory>

namespace dbus {
class Bus;
}

namespace ash {

class CrosDBusService;

// Owns and manages the lifetime of the ash D-Bus services.
class AshDBusServices {
 public:
  explicit AshDBusServices(dbus::Bus* system_bus);

  AshDBusServices(const AshDBusServices&) = delete;
  AshDBusServices& operator=(const AshDBusServices&) = delete;

  ~AshDBusServices();

 private:
  std::unique_ptr<CrosDBusService> display_service_;
  std::unique_ptr<CrosDBusService> gesture_properties_service_;
  std::unique_ptr<CrosDBusService> liveness_service_;
  std::unique_ptr<CrosDBusService> privacy_screen_service_;
  std::unique_ptr<CrosDBusService> url_handler_service_;
  std::unique_ptr<CrosDBusService> user_authentication_service_;
};

}  // namespace ash

#endif  // ASH_DBUS_ASH_DBUS_SERVICES_H_
