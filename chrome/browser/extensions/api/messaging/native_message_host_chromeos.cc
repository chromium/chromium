// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"
#include "chrome/browser/ash/drive/drivefs_native_message_host_ash.h"
#include "chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"
#include "chrome/browser/extensions/api/messaging/native_message_built_in_host.h"
#include "chrome/browser/extensions/api/messaging/native_message_echo_host.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/host/it2me/it2me_native_messaging_host_allowed_origins.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"

namespace extensions {

namespace {

std::unique_ptr<NativeMessageHost> CreateIt2MeHost(
    content::BrowserContext* browser_context) {
  return remoting::CreateIt2MeNativeMessagingHostForChromeOS();
}

}  // namespace

const NativeMessageBuiltInHost kBuiltInHosts[] = {
    {NativeMessageEchoHost::kHostName, NativeMessageEchoHost::kOrigins,
     NativeMessageEchoHost::kOriginCount, &NativeMessageEchoHost::Create},
    {remoting::kIt2MeNativeMessageHostName, remoting::kIt2MeOrigins,
     remoting::kIt2MeOriginsSize, &CreateIt2MeHost},
    {arc::ArcSupportMessageHost::kHostName,
     arc::ArcSupportMessageHost::kHostOrigin, 1,
     &arc::ArcSupportMessageHost::Create},
    {drive::kDriveFsNativeMessageHostName,
     drive::kDriveFsNativeMessageHostOrigins.data(),
     drive::kDriveFsNativeMessageHostOrigins.size(),
     &drive::CreateDriveFsNativeMessageHostAsh},
    {ash::guest_os::VmSKForwardingNativeMessageHost::kHostName,
     ash::guest_os::VmSKForwardingNativeMessageHost::kOrigins,
     ash::guest_os::VmSKForwardingNativeMessageHost::kOriginCount,
     &ash::guest_os::VmSKForwardingNativeMessageHost::CreateFromExtension},
};

const size_t kBuiltInHostsCount = std::size(kBuiltInHosts);

}  // namespace extensions
