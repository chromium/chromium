// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions {

// A port that manages communication with an Android application.
// All methods must be called on the UI Thread of the browser process.
class NativeMessageAndroidPort : public MessagePort {
 public:
  static std::unique_ptr<NativeMessageAndroidPort> Create(
      Profile* profile,
      base::WeakPtr<ChannelDelegate> channel_delegate,
      const PortId& port_id,
      const std::string& package_name,
      const ExtensionId& extension_id,
      std::string* error_out);

  ~NativeMessageAndroidPort() override;

  NativeMessageAndroidPort(const NativeMessageAndroidPort&) = delete;
  NativeMessageAndroidPort& operator=(const NativeMessageAndroidPort&) = delete;

  // MessagePort implementation.
  bool IsValidPort() override;
  void DispatchOnMessage(Message message) override;

  // Called by Java when the app this port is communicating with sends a message
  // back to the browser.
  void PostMessageFromApp(JNIEnv* env,
                          const base::android::JavaRef<jstring>& message);

  // Called by Java when the communication channel is closed by the app.
  void CloseChannel(JNIEnv* env,
                    const base::android::JavaRef<jstring>& error_message);

 private:
  NativeMessageAndroidPort(base::WeakPtr<ChannelDelegate> channel_delegate,
                           const PortId& port_id);

  // Initiates connection to the Android app. Returns an error message if
  // connection failed, or std::nullopt on success.
  std::optional<std::string> ConnectToApp(Profile* profile,
                                          const std::string& package_name,
                                          const ExtensionId& extension_id);

  base::android::ScopedJavaGlobalRef<jobject> java_peer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGE_ANDROID_PORT_H_
