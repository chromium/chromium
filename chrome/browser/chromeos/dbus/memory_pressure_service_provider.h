// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MEMORY_PRESSURE_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MEMORY_PRESSURE_SERVICE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace chromeos {

// This class exports D-Bus methods for handling memory pressure.
//
// This service can be manually tested using dbus-send:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.MemoryPressure /org/chromium/MemoryPressure
//     org.chromium.MemoryPressure.GetAvailableMemoryKB
class MemoryPressureServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  MemoryPressureServiceProvider();
  MemoryPressureServiceProvider& operator=(
      const MemoryPressureServiceProvider&) = delete;
  MemoryPressureServiceProvider(const MemoryPressureServiceProvider&) = delete;
  ~MemoryPressureServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void GetAvailableMemoryKB(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void GetMemoryMarginsKB(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  // TODO(b/149833548): Implement signals CriticalMemoryPressure and
  // ModerateMemoryPressure.

  base::WeakPtrFactory<MemoryPressureServiceProvider> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MEMORY_PRESSURE_SERVICE_PROVIDER_H_
