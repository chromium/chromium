// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHUTDOWN_CONTROLLER_IMPL_H_
#define ASH_SHUTDOWN_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shutdown_controller.h"
#include "base/observer_list.h"

namespace ash {

enum class ShutdownReason;

// Handles actual device shutdown by making requests to powerd over D-Bus.
// Caches the DeviceRebootOnShutdown device policy sent from Chrome.
class ASH_EXPORT ShutdownControllerImpl : public ShutdownController {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when shutdown policy changes.
    virtual void OnShutdownPolicyChanged(bool reboot_on_shutdown) = 0;
  };

  ShutdownControllerImpl();

  ShutdownControllerImpl(const ShutdownControllerImpl&) = delete;
  ShutdownControllerImpl& operator=(const ShutdownControllerImpl&) = delete;

  ~ShutdownControllerImpl() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool reboot_on_shutdown() const { return reboot_on_shutdown_; }

  // ShutdownController:
  void SetRebootOnShutdown(bool reboot_on_shutdown) override;
  void ShutDownOrReboot(ShutdownReason reason) override;

 private:
  // Cached copy of the DeviceRebootOnShutdown policy from chrome.
  bool reboot_on_shutdown_ = false;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SHUTDOWN_CONTROLLER_IMPL_H_
