// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_DISK_MANAGEMENT_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_DISK_MANAGEMENT_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

using ExpectedGetDiskInfoResponse =
    base::expected<borealis::BorealisDiskManager::GetDiskInfoResponse,
                   borealis::Described<borealis::BorealisGetDiskInfoResult>>;

using ExpectedRequestDeltaResponse =
    base::expected<uint64_t,
                   borealis::Described<borealis::BorealisResizeDiskResult>>;

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports D-Bus methods used by Crostini VMs (currently only
// Borealis) for resizing their disks.
class VmDiskManagementServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmDiskManagementServiceProvider();
  VmDiskManagementServiceProvider(const VmDiskManagementServiceProvider&) =
      delete;
  VmDiskManagementServiceProvider& operator=(
      const VmDiskManagementServiceProvider&) = delete;
  ~VmDiskManagementServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when UpdateApplicationList() is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void GetDiskInfo(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);
  void RequestSpace(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);
  void ReleaseSpace(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Callbacks for responding to certain D-Bus requests.
  void OnGetDiskInfo(std::unique_ptr<dbus::Response> response,
                     dbus::ExportedObject::ResponseSender response_sender,
                     ExpectedGetDiskInfoResponse response_or_error);
  void OnRequestSpace(std::unique_ptr<dbus::Response> response,
                      dbus::ExportedObject::ResponseSender response_sender,
                      ExpectedRequestDeltaResponse response_or_error);
  void OnReleaseSpace(std::unique_ptr<dbus::Response> response,
                      dbus::ExportedObject::ResponseSender response_sender,
                      ExpectedRequestDeltaResponse response_or_error);

  base::WeakPtrFactory<VmDiskManagementServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_DISK_MANAGEMENT_SERVICE_PROVIDER_H_
