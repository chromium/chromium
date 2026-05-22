// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A port that manages communication with an Android application.
// All methods must be called on the UI Thread of the browser process.
class NativeMessageAndroidPort : public MessagePort {
 public:
  NativeMessageAndroidPort(content::BrowserContext* browser_context,
                           base::WeakPtr<ChannelDelegate> channel_delegate,
                           const PortId& port_id,
                           const ExtensionId& extension_id,
                           const std::string& package_name);
  ~NativeMessageAndroidPort() override;

  NativeMessageAndroidPort(const NativeMessageAndroidPort&) = delete;
  NativeMessageAndroidPort& operator=(const NativeMessageAndroidPort&) = delete;

  // MessagePort implementation.
  bool IsValidPort() override;
  void DispatchOnMessage(Message message) override;

  // Called when the app this port is communicating with sends a message back to
  // the browser.
  void PostMessageFromApp(const std::string& message);

  // Called when the communication channel is closed by the app.
  void CloseChannel(const std::string& error_message);

 private:
  base::WeakPtrFactory<NativeMessageAndroidPort> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_
