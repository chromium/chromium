// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_DISPLAY_SERVICE_PROVIDER_H_
#define ASH_DBUS_DISPLAY_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace ash {

// This class implements org.chromium.DisplayService for chrome.
class DisplayServiceProvider
    : public chromeos::CrosDBusService::ServiceProviderInterface {
 public:
  // The caller must ensure that |delegate| outlives this object.
  DisplayServiceProvider();
  ~DisplayServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  class Impl;

  // Forwards DBus calls to DisplayConfigurator::SetDisplayPower.
  void SetDisplayPower(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender);

  // Forwards DBus calls to ScreenDimmer::SetDimming.
  void SetDisplaySoftwareDimming(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Called when a external process no longer needs control of the display and
  // Chrome can take ownership. Forwarded to mojom::AshDisplayController.
  void TakeDisplayOwnership(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Called when a external process needs control of the display and needs
  // Chrome to release ownership. Forwarded to mojom::AshDisplayController.
  void ReleaseDisplayOwnership(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  std::unique_ptr<Impl> impl_;
  base::WeakPtrFactory<DisplayServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayServiceProvider);
};

}  // namespace ash

#endif  // ASH_DBUS_DISPLAY_SERVICE_PROVIDER_H_
