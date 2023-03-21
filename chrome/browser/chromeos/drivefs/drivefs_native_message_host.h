// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_H_
#define CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace extensions {
class NativeMessageHost;
}

class Profile;

namespace drive {

// This callback is used by the native message host to initiate a connection
// with DriveFS.
using CreateNativeHostSessionCallback = base::OnceCallback<void(
    drivefs::mojom::ExtensionConnectionParamsPtr,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost>,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingPort>)>;

// Called when an extension wants to initiate a connection with DriveFS. This
// function creates a native message host, which will call `callback` when it
// has set up the mojo pipes for communication and wants to send the endpoints
// to DriveFS.
std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    CreateNativeHostSessionCallback callback);

// Exposed for testing purposes only. Used internally by
// `ConnectToDriveFsNativeMessageExtension` to construct a native message host.
std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsInitiatedNativeMessageHostInternal(
    Profile* profile,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote);

// Called when DriveFS wants to initiate a connection to an extension. This
// creates a native message host for the given `extension_id` and passes the
// mojo endpoints to the host to create a connection.
drivefs::mojom::ExtensionConnectionStatus
ConnectToDriveFsNativeMessageExtension(
    Profile* profile,
    const std::string& extension_id,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote);

}  // namespace drive

#endif  //  CHROME_BROWSER_CHROMEOS_DRIVEFS_DRIVEFS_NATIVE_MESSAGE_HOST_H_
