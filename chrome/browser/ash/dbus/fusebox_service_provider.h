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

  // D-Bus methods.
  //
  // In terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  void Close(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender sender);
  void MkDir(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender sender);
  void Open(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);
  void Read(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);
  void ReadDir2(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender sender);
  void RmDir(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender sender);
  void Stat(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);

  void ListStorages(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender sender);

  scoped_refptr<dbus::ExportedObject> exported_object_;
  fusebox::Server server_;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
