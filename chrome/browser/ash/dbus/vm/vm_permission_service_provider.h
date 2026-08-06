// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_PERMISSION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_PERMISSION_SERVICE_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// Exports D-Bus methods for registering VMs and managing their permissions.
class VmPermissionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmPermissionServiceProvider();

  VmPermissionServiceProvider(const VmPermissionServiceProvider&) = delete;
  VmPermissionServiceProvider& operator=(const VmPermissionServiceProvider&) =
      delete;

  ~VmPermissionServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  struct VmInfo {
    enum VmType { CrostiniVm = 0, Borealis = 2, Bruschetta = 3 };
    enum PermissionType { PermissionCamera = 0, PermissionMicrophone = 1 };

    const std::string owner_id;
    const std::string name;
    const VmType type;

    base::flat_map<PermissionType, bool> permission_to_enabled_map;

    VmInfo(std::string owner_id, std::string name, VmType type);
    ~VmInfo();
  };

  using VmMap = std::unordered_map<base::UnguessableToken,
                                   std::unique_ptr<VmInfo>,
                                   base::UnguessableTokenHash>;

  // Called from ExportedObject when GetLicenseDataResponse() is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void RegisterVm(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);

  void UnregisterVm(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  void SetPermissions(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  void GetPermissions(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  void UpdateVmPermissions(VmInfo* vm);
  void UpdateBorealisPermissions(VmInfo* vm);
  void UpdateBruschettaPermissions(VmInfo* vm);

  void SetCameraPermission(base::UnguessableToken token, bool enabled);

  // Returns an iterator to a vm with given |owner_id| and |name|).
  VmMap::iterator FindVm(const std::string& owner_id, const std::string& name);

  // VMs currently registered with the permission service, keyed by their
  // access token.
  VmMap vms_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<VmPermissionServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_PERMISSION_SERVICE_PROVIDER_H_
