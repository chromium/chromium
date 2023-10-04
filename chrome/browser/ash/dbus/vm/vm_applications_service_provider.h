// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_APPLICATIONS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_APPLICATIONS_SERVICE_PROVIDER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports D-Bus methods for functions that we want to be available
// to the Crostini container.
class VmApplicationsServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmApplicationsServiceProvider();

  VmApplicationsServiceProvider(const VmApplicationsServiceProvider&) = delete;
  VmApplicationsServiceProvider& operator=(
      const VmApplicationsServiceProvider&) = delete;

  ~VmApplicationsServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(VmApplicationsServiceProviderTest,
                           ParseSelectFileDialogFileTypes);

  // Called from ExportedObject when UpdateApplicationList() is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void UpdateApplicationList(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);
  void LaunchTerminal(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);
  void UpdateMimeTypes(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender);
  void SelectFile(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);

  // Exposed in the header so unit tests can call it.
  static void ParseSelectFileDialogFileTypes(
      const std::string& allowed_extensions,
      ui::SelectFileDialog::FileTypeInfo* file_types,
      int* file_type_index);

  base::WeakPtrFactory<VmApplicationsServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_APPLICATIONS_SERVICE_PROVIDER_H_
