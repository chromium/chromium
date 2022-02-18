// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace ash {

// FuseBoxServiceProvider implements the org.chromium.FuseBoxService D-Bus
// interface.
class FuseBoxServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  FuseBoxServiceProvider();
  FuseBoxServiceProvider(const FuseBoxServiceProvider&) = delete;
  FuseBoxServiceProvider& operator=(const FuseBoxServiceProvider&) = delete;
  ~FuseBoxServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> object) override;

 private:
  // D-Bus methods.
  //
  // In terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  //
  // TODO(nigeltao): add Open, ReadDir, etc.
  void Read(dbus::MethodCall* method,
            dbus::ExportedObject::ResponseSender sender);
  void Stat(dbus::MethodCall* method,
            dbus::ExportedObject::ResponseSender sender);

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
