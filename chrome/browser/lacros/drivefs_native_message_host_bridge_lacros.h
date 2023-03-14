// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_BRIDGE_LACROS_H_
#define CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_BRIDGE_LACROS_H_

#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom-forward.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace drive {

// Created in lacros-chrome. Allows DriveFS to connect to an extension in lacros
// (via ash).
class DriveFsNativeMessageHostBridge
    : public crosapi::mojom::DriveFsNativeMessageHostBridge {
 public:
  DriveFsNativeMessageHostBridge();
  DriveFsNativeMessageHostBridge(const DriveFsNativeMessageHostBridge&) =
      delete;
  DriveFsNativeMessageHostBridge& operator=(
      const DriveFsNativeMessageHostBridge&) = delete;
  ~DriveFsNativeMessageHostBridge() override;

 private:
  void ConnectToExtension(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
          extension_receiver,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote,
      ConnectToExtensionCallback callback) override;

  mojo::Receiver<crosapi::mojom::DriveFsNativeMessageHostBridge> receiver_;
};

}  // namespace drive

#endif  //  CHROME_BROWSER_LACROS_DRIVEFS_NATIVE_MESSAGE_HOST_BRIDGE_LACROS_H_
