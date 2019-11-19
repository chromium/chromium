// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_DRIVE_FILE_STREAM_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_DRIVE_FILE_STREAM_SERVICE_PROVIDER_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace chromeos {

// DriveFileStreamServiceProvider exposes D-Bus methods which will be
// called by the DriveFileStream service - new implementation of Drive
// cloud storage support for ChromeOS. It allows establishing IPC between
// system service and the browser delegating auth and UI interactions
// to the browser.
class DriveFileStreamServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  DriveFileStreamServiceProvider();
  ~DriveFileStreamServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called on UI thread to handle incoming D-Bus method calls.
  void HandleOpenIpcChannel(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<DriveFileStreamServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveFileStreamServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_DRIVE_FILE_STREAM_SERVICE_PROVIDER_H_
