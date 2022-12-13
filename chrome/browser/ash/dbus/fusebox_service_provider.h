// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {

// FuseBoxServiceProvider implements the org.chromium.FuseBoxService D-Bus
// interface.
class FuseBoxServiceProvider : public CrosDBusService::ServiceProviderInterface,
                               public fusebox::Server::Delegate {
 public:
  FuseBoxServiceProvider();
  FuseBoxServiceProvider(const FuseBoxServiceProvider&) = delete;
  FuseBoxServiceProvider& operator=(const FuseBoxServiceProvider&) = delete;
  ~FuseBoxServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> object) override;

 private:
  // fusebox::Server::Delegate overrides:
  void OnRegisterFSURLPrefix(const std::string& subdir) override;
  void OnUnregisterFSURLPrefix(const std::string& subdir) override;

  // D-Bus template methods.

  template <typename RequestProto, typename ResponseProto>
  using ServerMethodPtr = void (fusebox::Server::*)(
      const RequestProto& request,
      base::OnceCallback<void(const ResponseProto& response)> callback);

  template <typename RequestProto, typename ResponseProto>
  void ServeProtoMethod(ServerMethodPtr<RequestProto, ResponseProto> method,
                        dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender sender);

  template <typename RequestProto, typename ResponseProto>
  void ExportProtoMethod(const std::string& method_name,
                         ServerMethodPtr<RequestProto, ResponseProto> method);

  // Private fields.

  scoped_refptr<dbus::ExportedObject> exported_object_;
  fusebox::Server server_;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
