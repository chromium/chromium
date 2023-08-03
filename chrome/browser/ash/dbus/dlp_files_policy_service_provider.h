// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_DLP_FILES_POLICY_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_DLP_FILES_POLICY_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

class DlpFilesPolicyServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  DlpFilesPolicyServiceProvider();
  ~DlpFilesPolicyServiceProvider() override;
  DlpFilesPolicyServiceProvider(const DlpFilesPolicyServiceProvider&) = delete;
  DlpFilesPolicyServiceProvider& operator=(
      const DlpFilesPolicyServiceProvider&) = delete;

  // CrosDBusService::ServiceProviderInterface override:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Callback called when method is exported or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // org.chromium.DlpFilesPolicyService.IsDlpPolicyMatched implementation.
  void IsDlpPolicyMatched(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  // org.chromium.DlpFilesPolicyService.IsFilesTransferRestricted
  // implementation.
  void IsFilesTransferRestricted(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<DlpFilesPolicyServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_DLP_FILES_POLICY_SERVICE_PROVIDER_H_
