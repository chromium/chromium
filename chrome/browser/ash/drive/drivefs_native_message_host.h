// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace drive {

extern const char kDriveFsNativeMessageHostName[];

extern const char* const kDriveFsNativeMessageHostOrigins[];

extern const size_t kDriveFsNativeMessageHostOriginsSize;

std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    content::BrowserContext* browser_context);

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsInitiatedNativeMessageHost(
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote);

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostForTesting(
    drivefs::mojom::DriveFs* drivefs_for_testing);

drivefs::mojom::DriveFsDelegate::ExtensionConnectionStatus
ConnectToDriveFsNativeMessageExtension(
    Profile* profile,
    const std::string& extension_id,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote);

}  // namespace drive

#endif  //  CHROME_BROWSER_ASH_DRIVE_DRIVEFS_NATIVE_MESSAGE_HOST_H_
