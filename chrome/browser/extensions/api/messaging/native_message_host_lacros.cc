// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/extensions/api/messaging/native_message_built_in_host.h"
#include "chrome/browser/extensions/api/messaging/native_message_echo_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/host/it2me/it2me_native_messaging_host_allowed_origins.h"
#include "remoting/host/it2me/it2me_native_messaging_host_lacros.h"

namespace extensions {

namespace {

std::unique_ptr<NativeMessageHost> CreateIt2MeHost(
    content::BrowserContext* browser_context) {
  return remoting::CreateIt2MeNativeMessagingHostForLacros(
      content::GetUIThreadTaskRunner({}));
}

}  // namespace

const NativeMessageBuiltInHost kBuiltInHosts[] = {
    {NativeMessageEchoHost::kHostName, NativeMessageEchoHost::kOrigins,
     NativeMessageEchoHost::kOriginCount, &NativeMessageEchoHost::Create},
    {remoting::kIt2MeNativeMessageHostName, remoting::kIt2MeOrigins,
     remoting::kIt2MeOriginsSize, &CreateIt2MeHost},
};

const size_t kBuiltInHostsCount = std::size(kBuiltInHosts);

}  // namespace extensions
