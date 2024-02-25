// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_NATIVE_MESSAGE_HOST_H_
#define CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_NATIVE_MESSAGE_HOST_H_

// This file is copied from
// //chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.h

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace {
class BrowserContext;
}

class Profile;

namespace guest_os {

// Implements a message pipe to communicate with a Chrome Extension for
// Security Key forwarding.
// Supports only one-off request-response message exchange initiated from
// Chrome side. It sends the message to the extension once message channel is
// opened and closes the channel once a response is received.
class VmSKForwardingNativeMessageHost : public extensions::NativeMessageHost {
 public:
  static const char* const kHostName;
  static const char* const kOrigins[];
  static const char* const kHostCreatedByExtensionNotSupportedError;
  static const size_t kOriginCount;

  using ResponseCallback =
      base::OnceCallback<void(const std::string& response)>;

  // Used when extension tries to create a message channel with NM Host.
  // Extension-initiated communication is not supported for SK forwarding.
  // However, this method is required by NativeMessageBuiltInHost to register
  // the NM host in Chrome OS. Created NM host will post an error message to the
  // extension (for debugging) and close the channel.
  static std::unique_ptr<NativeMessageHost> CreateFromExtension(
      content::BrowserContext* browser_context);

  // Meant to be used by for communication initiated from Chrome side (e.g. from
  // D-Bus service provider).
  // Created instance sends |json_message| to the extension once communication
  // channel is opened and uses |response_callback| to return the response
  // message from the extension.
  static std::unique_ptr<NativeMessageHost> CreateFromDBus(
      const std::string& json_message,
      VmSKForwardingNativeMessageHost::ResponseCallback response_callback);

  // Delivers |json_message| to the first enabled extension from kOrigins.
  // It creates a new native message host for this one-off delivery.
  // |response_callback| will be called with the response from the extension.
  static void DeliverMessageToSKForwardingExtension(
      Profile* profile,
      const std::string& json_message,
      VmSKForwardingNativeMessageHost::ResponseCallback response_callback);

  // Prefer using CreateFrom*() methods to this constructor.
  VmSKForwardingNativeMessageHost(const std::string& json_message_to_send,
                                  ResponseCallback response_callback);
  VmSKForwardingNativeMessageHost(const VmSKForwardingNativeMessageHost&) =
      delete;
  VmSKForwardingNativeMessageHost& operator=(
      const VmSKForwardingNativeMessageHost&) = delete;
  ~VmSKForwardingNativeMessageHost() override;

  // NativeMessageHost implementation.
  void Start(extensions::NativeMessageHost::Client* client) override;
  void OnMessage(const std::string& request_string) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  static void DeliverMessageToExtensionByID(
      Profile* profile,
      const std::string& extension_id,
      const std::string& json_message,
      base::OnceCallback<void(const std::string& response)> response_callback);

  // Callback for sending message back to the caller once extension responses.
  ResponseCallback response_callback_;

  // Message to be sent to the extension once communication channel is set up.
  const std::string json_message_to_send_;

  // Unowned. |client_| must outlive this instance.
  raw_ptr<extensions::NativeMessageHost::Client> client_ = nullptr;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_NATIVE_MESSAGE_HOST_H_
