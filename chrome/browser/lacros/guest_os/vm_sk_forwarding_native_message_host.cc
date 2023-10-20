// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is copied from
// //chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.cc

#include "chrome/browser/lacros/guest_os/vm_sk_forwarding_native_message_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "url/gurl.h"

namespace guest_os {

// static
const char* const VmSKForwardingNativeMessageHost::kHostName =
    "com.google.vm_sk_forwarding";

// static
const char* const VmSKForwardingNativeMessageHost::kOrigins[] = {
    "chrome-extension://lehkgnicackihfeppclgiffgbgbhmbdp/",
    "chrome-extension://lcooaekmckohjjnpaaokodoepajbnill/"};

// static
const char* const
    VmSKForwardingNativeMessageHost::kHostCreatedByExtensionNotSupportedError =
        "{\"error\":\"Communication initiated by extension is not "
        "supported.\"}";

// static
const size_t VmSKForwardingNativeMessageHost::kOriginCount =
    std::size(kOrigins);

// static
std::unique_ptr<extensions::NativeMessageHost>
VmSKForwardingNativeMessageHost::CreateFromExtension(
    content::BrowserContext* browser_context) {
  ResponseCallback do_nothing = base::DoNothing();
  return std::make_unique<VmSKForwardingNativeMessageHost>(
      kHostCreatedByExtensionNotSupportedError, std::move(do_nothing));
}

// static
std::unique_ptr<extensions::NativeMessageHost>
VmSKForwardingNativeMessageHost::CreateFromDBus(
    const std::string& json_message,
    VmSKForwardingNativeMessageHost::ResponseCallback response_callback) {
  return std::make_unique<VmSKForwardingNativeMessageHost>(
      json_message, std::move(response_callback));
}

VmSKForwardingNativeMessageHost::VmSKForwardingNativeMessageHost(
    const std::string& json_message_to_send,
    ResponseCallback response_callback)
    : response_callback_(std::move(response_callback)),
      json_message_to_send_(json_message_to_send) {
  DCHECK(response_callback_);
}

VmSKForwardingNativeMessageHost::~VmSKForwardingNativeMessageHost() {
  if (response_callback_) {
    // If no response was received from the extension, pass the empty result
    // to the callback to signal the error.
    std::move(response_callback_).Run(std::string() /* response */);
  }
}

void VmSKForwardingNativeMessageHost::Start(
    extensions::NativeMessageHost::Client* client) {
  DCHECK(!client_);
  client_ = client;
  if (!json_message_to_send_.empty()) {
    client_->PostMessageFromNativeHost(json_message_to_send_);
  }
}

void VmSKForwardingNativeMessageHost::OnMessage(const std::string& message) {
  if (!response_callback_) {
    // This happens when the extension sent more than one message via the
    // message channel. This class doesn't support this, thus we
    // discard extra messages.
    VLOG(1) << "VmSKForwardingNativeMessageHost received an extra message. "
            << "Discarding the message.";
    return;
  }
  DCHECK(client_);

  std::move(response_callback_).Run(message /* response */);
  client_->CloseChannel(std::string() /* error_message */);
}

scoped_refptr<base::SingleThreadTaskRunner>
VmSKForwardingNativeMessageHost::task_runner() const {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

void VmSKForwardingNativeMessageHost::DeliverMessageToExtensionByID(
    Profile* profile,
    const std::string& extension_id,
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)> response_callback) {
  const extensions::PortId port_id(
      base::UnguessableToken::Create(), 1 /* port_number */,
      true /* is_opener */, extensions::mojom::SerializationFormat::kJson);

  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);

  auto native_message_host = VmSKForwardingNativeMessageHost::CreateFromDBus(
      json_message, std::move(response_callback));

  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));

  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(
          VmSKForwardingNativeMessageHost::kHostName),
      std::move(native_message_port), extension_id, GURL(),
      extensions::mojom::ChannelType::kNative,
      std::string() /* channel_name */);
}

void VmSKForwardingNativeMessageHost::DeliverMessageToSKForwardingExtension(
    Profile* profile,
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)> response_callback) {
  DCHECK(profile);
  DCHECK(response_callback);

  // Send the message to the first enabled extension from the origins list.
  for (const auto* extension_url : VmSKForwardingNativeMessageHost::kOrigins) {
    GURL url = GURL(extension_url);
    if (extensions::ExtensionRegistry::Get(profile)
            ->enabled_extensions()
            .GetExtensionOrAppByURL(url)) {
      DeliverMessageToExtensionByID(profile, url.host(), json_message,
                                    std::move(response_callback));
      return;
    }
  }

  // No extension to accept the message. Pass the empty result to the callback
  // to signal the error.
  std::move(response_callback).Run("" /* response */);
}

}  // namespace guest_os
