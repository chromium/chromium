// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/android/native_message_android_port.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "extensions/common/api/messaging/message.h"

namespace extensions {

NativeMessageAndroidPort::NativeMessageAndroidPort(
    content::BrowserContext* browser_context,
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const ExtensionId& extension_id,
    const std::string& package_name)
    : MessagePort(std::move(channel_delegate), port_id) {}

NativeMessageAndroidPort::~NativeMessageAndroidPort() = default;

bool NativeMessageAndroidPort::IsValidPort() {
  return true;
}

void NativeMessageAndroidPort::DispatchOnMessage(Message message) {}

void NativeMessageAndroidPort::PostMessageFromApp(const std::string& message) {
  if (weak_channel_delegate_) {
    weak_channel_delegate_->PostMessage(
        port_id_, Message(message, /*user_gesture=*/false));
  }
}

void NativeMessageAndroidPort::CloseChannel(const std::string& error_message) {
  if (weak_channel_delegate_) {
    weak_channel_delegate_->CloseChannel(port_id_, error_message);
  }
}

}  // namespace extensions
