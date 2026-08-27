// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/android/native_message_android_port.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/extensions/api/messaging/android/jni_headers/NativeMessageAndroidPort_jni.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension.h"

namespace extensions {

std::unique_ptr<NativeMessageAndroidPort> NativeMessageAndroidPort::Create(
    Profile* profile,
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const std::string& package_name,
    const ExtensionId& extension_id,
    const SigningCertificates& android_certificates,
    std::string* error_out) {
  CHECK(error_out);

  std::unique_ptr<NativeMessageAndroidPort> port(
      new NativeMessageAndroidPort(std::move(channel_delegate), port_id));
  std::optional<std::string> error =
      port->ConnectToApp(profile, package_name, extension_id);
  if (error.has_value()) {
    *error_out = std::move(*error);
    return nullptr;
  }
  return port;
}

NativeMessageAndroidPort::NativeMessageAndroidPort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id)
    : MessagePort(std::move(channel_delegate), port_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_peer_.Reset(Java_NativeMessageAndroidPort_create(
      env, reinterpret_cast<intptr_t>(this)));
}

std::optional<std::string> NativeMessageAndroidPort::ConnectToApp(
    Profile* profile,
    const std::string& package_name,
    const ExtensionId& extension_id) {
  const Extension* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  CHECK(extension);

  // The extension is verified if its contents have been verified against a
  // source of truth and content verification is currently active.
  ChromeContentVerifierDelegate delegate(profile);

  // Consider both SIGNED_HASHES and UNSIGNED_HASHES as verified.
  // UNSIGNED_HASHES is for extensions which have their hashes calculated at
  // install time and are repaired in case of corruption. This behavior is
  // reserved only for policy installed extensions.
  bool is_verified = delegate.GetVerifierSourceType(*extension) !=
                     ContentVerifierDelegate::VerifierSourceType::NONE;

  return Java_NativeMessageAndroidPort_connectToApp(
      base::android::AttachCurrentThread(), java_peer_, profile, package_name,
      extension_id, is_verified);
}

NativeMessageAndroidPort::~NativeMessageAndroidPort() {
  if (!java_peer_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_NativeMessageAndroidPort_destroy(env, java_peer_);
    java_peer_.Reset();
  }
}

bool NativeMessageAndroidPort::IsValidPort() {
  return true;
}

void NativeMessageAndroidPort::DispatchOnMessage(Message message) {
  Java_NativeMessageAndroidPort_forwardMessageToApp(
      base::android::AttachCurrentThread(), java_peer_, message.data());
}

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

DEFINE_JNI(NativeMessageAndroidPort)
