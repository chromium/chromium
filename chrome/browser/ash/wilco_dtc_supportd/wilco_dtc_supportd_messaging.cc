// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_messaging.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/channel_type.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/serialization_format.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/system/handle.h"
#include "url/gurl.h"

class Profile;

namespace ash {

// List of extension URLs that will communicate with wilco_dtc
// through the extensions native messaging system.
//
// Note: the list size must be kept in sync with
// |kWilcoDtcSupportdHostOriginsSize|.
const char* const kWilcoDtcSupportdHostOrigins[] = {
    "chrome-extension://emelalhagcpibaiiiijjlkmhhbekaidg/"};

// Size of |kWilcoDtcSupportdHostOrigins| array.
const size_t kWilcoDtcSupportdHostOriginsSize =
    std::size(kWilcoDtcSupportdHostOrigins);

// Native application name that is used for passing UI messages between the
// wilco_dtc daemon and extensions.
const char kWilcoDtcSupportdUiMessageHost[] = "com.google.wilco_dtc";

// Error messages sent to the extension:
const char kWilcoDtcSupportdUiMessageTooBigExtensionsError[] =
    "Message is too big.";
const char kWilcoDtcSupportdUiExtraMessagesExtensionsError[] =
    "At most one message must be sent through the message channel.";

// Maximum allowed size of UI messages passed between the wilco_dtc daemon and
// extensions.
const int kWilcoDtcSupportdUiMessageMaxSize = 1000000;

namespace {

// Extensions native message host implementation that is used when an
// extension requests a message channel to the wilco_dtc daemon.
//
// The message is transmitted via the wilco_dtc_supportd daemon. One instance of
// this class allows only one message to be sent; at most one message will be
// sent in the reverse direction: it will contain the daemon's response.
class WilcoDtcSupportdExtensionOwnedMessageHost final
    : public extensions::NativeMessageHost {
 public:
  WilcoDtcSupportdExtensionOwnedMessageHost() = default;

  WilcoDtcSupportdExtensionOwnedMessageHost(
      const WilcoDtcSupportdExtensionOwnedMessageHost&) = delete;
  WilcoDtcSupportdExtensionOwnedMessageHost& operator=(
      const WilcoDtcSupportdExtensionOwnedMessageHost&) = delete;

  ~WilcoDtcSupportdExtensionOwnedMessageHost() override = default;

  // extensions::NativeMessageHost:

  void Start(Client* client) override {
    DCHECK(!client_);
    client_ = client;
  }

  void OnMessage(const std::string& request_string) override {
    DCHECK(client_);

    if (is_disposed_) {
      // We already called CloseChannel() before so ignore messages arriving at
      // this point. This corner case can happen because CloseChannel() does its
      // job asynchronously.
      return;
    }

    if (message_from_extension_received_) {
      // Our implementation doesn't allow sending multiple messages from the
      // extension over the same instance.
      DisposeSelf(kWilcoDtcSupportdUiExtraMessagesExtensionsError);
      return;
    }
    message_from_extension_received_ = true;

    if (request_string.size() > kWilcoDtcSupportdUiMessageMaxSize) {
      DisposeSelf(kWilcoDtcSupportdUiMessageTooBigExtensionsError);
      return;
    }

    WilcoDtcSupportdBridge* const wilco_dtc_supportd_bridge =
        WilcoDtcSupportdBridge::Get();
    if (!wilco_dtc_supportd_bridge) {
      VLOG(0) << "Cannot send message - no bridge to the daemon";
      DisposeSelf(kNotFoundError);
      return;
    }

    chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceProxy* const
        wilco_dtc_supportd_mojo_proxy =
            wilco_dtc_supportd_bridge->wilco_dtc_supportd_service_mojo_proxy();
    if (!wilco_dtc_supportd_mojo_proxy) {
      VLOG(0) << "Cannot send message - Mojo connection to the daemon isn't "
                 "bootstrapped yet";
      DisposeSelf(kNotFoundError);
      return;
    }

    mojo::ScopedHandle json_message_mojo_handle =
        MojoUtils::CreateReadOnlySharedMemoryMojoHandle(request_string);
    if (!json_message_mojo_handle) {
      LOG(ERROR) << "Cannot create Mojo shared memory handle from string";
      DisposeSelf(kHostInputOutputError);
      return;
    }

    wilco_dtc_supportd_mojo_proxy->SendUiMessageToWilcoDtc(
        std::move(json_message_mojo_handle),
        base::BindOnce(&WilcoDtcSupportdExtensionOwnedMessageHost::
                           OnResponseReceivedFromDaemon,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  void DisposeSelf(const std::string& error_message) {
    DCHECK(!is_disposed_);
    is_disposed_ = true;
    client_->CloseChannel(error_message);
    // Prevent the Mojo call result, if it's still in flight, from being
    // forwarded to the extension.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void OnResponseReceivedFromDaemon(mojo::ScopedHandle response_json_message) {
    DCHECK(client_);
    DCHECK(!is_disposed_);

    if (!response_json_message) {
      // The call to the wilco_dtc daemon failed or the daemon provided no
      // response, so just close the extension message channel as it's intended
      // to be used for one-time messages only.
      VLOG(1) << "Empty response, closing the extension message channel";
      DisposeSelf(std::string() /* error_message */);
      return;
    }

    base::ReadOnlySharedMemoryMapping response_json_shared_memory;
    base::StringPiece response_json_string =
        MojoUtils::GetStringPieceFromMojoHandle(
            std::move(response_json_message), &response_json_shared_memory);
    if (response_json_string.empty()) {
      LOG(ERROR) << "Cannot read response from Mojo shared memory";
      DisposeSelf(kHostInputOutputError);
      return;
    }

    if (response_json_string.size() > kWilcoDtcSupportdUiMessageMaxSize) {
      LOG(ERROR) << "The message received from the daemon is too big";
      DisposeSelf(kWilcoDtcSupportdUiMessageTooBigExtensionsError);
      return;
    }

    if (response_json_string.size() > kWilcoDtcSupportdUiMessageMaxSize) {
      LOG(ERROR) << "The message received from the daemon is too big";
      client_->CloseChannel(kHostInputOutputError);
      return;
    }

    client_->PostMessageFromNativeHost(std::string(response_json_string));
    DisposeSelf(std::string() /* error_message */);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  // Unowned.
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #constexpr-ctor-field-initializer
  RAW_PTR_EXCLUSION Client* client_ = nullptr;

  // Whether a message has already been received from the extension.
  bool message_from_extension_received_ = false;
  // Whether DisposeSelf() has already been called.
  bool is_disposed_ = false;

  // Must be the last member.
  base::WeakPtrFactory<WilcoDtcSupportdExtensionOwnedMessageHost>
      weak_ptr_factory_{this};
};

// Extensions native message host implementation that is used when the wilco_dtc
// daemon sends (via the wilco_dtc_supportd daemon) a message to the extension.
//
// A new instance of this class should be created for each instance of the
// extension(s) that are allowed to receive messages from the wilco_dtc daemon.
// Once the extension responds by posting a message back to this message
// channel, |send_response_callback| will be called.
class WilcoDtcSupportdDaemonOwnedMessageHost final
    : public extensions::NativeMessageHost {
 public:
  WilcoDtcSupportdDaemonOwnedMessageHost(
      const std::string& json_message_to_send,
      base::OnceCallback<void(const std::string& response)>
          send_response_callback)
      : json_message_to_send_(json_message_to_send),
        send_response_callback_(std::move(send_response_callback)) {
    DCHECK(send_response_callback_);
  }

  WilcoDtcSupportdDaemonOwnedMessageHost(
      const WilcoDtcSupportdDaemonOwnedMessageHost&) = delete;
  WilcoDtcSupportdDaemonOwnedMessageHost& operator=(
      const WilcoDtcSupportdDaemonOwnedMessageHost&) = delete;

  ~WilcoDtcSupportdDaemonOwnedMessageHost() override {
    if (send_response_callback_) {
      // If no response was received from the extension, pass the empty result
      // to the callback to signal the error.
      std::move(send_response_callback_).Run(std::string() /* response */);
    }
  }

  // extensions::NativeMessageHost:

  void Start(Client* client) override {
    DCHECK(!client_);
    client_ = client;
    client_->PostMessageFromNativeHost(json_message_to_send_);
  }

  void OnMessage(const std::string& request_string) override {
    DCHECK(client_);
    if (!send_response_callback_) {
      // This happens when the extension sent more than one message via the
      // message channel, which is not supported in our case - therefore simply
      // discard these extra messages.
      return;
    }
    if (request_string.size() > kWilcoDtcSupportdUiMessageMaxSize) {
      std::move(send_response_callback_).Run(std::string() /* response */);
      client_->CloseChannel(kWilcoDtcSupportdUiMessageTooBigExtensionsError);
      return;
    }
    std::move(send_response_callback_).Run(request_string /* response */);
    client_->CloseChannel(std::string() /* error_message */);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  const std::string json_message_to_send_;
  base::OnceCallback<void(const std::string& response)> send_response_callback_;
  // Unowned.
  raw_ptr<Client, ExperimentalAsh> client_ = nullptr;
};

// Helper that wraps the specified OnceCallback and encapsulates logic that
// executes it once either of the following becomes true (whichever happens to
// be earlier):
// * Non-empty data was provided to this class via the ProcessResponse() method;
// * The ProcessResponse() method has been called the |wrapper_callback_count|
//   number of times.
class FirstNonEmptyMessageCallbackWrapper final {
 public:
  FirstNonEmptyMessageCallbackWrapper(
      base::OnceCallback<void(const std::string& response)> original_callback,
      int wrapper_callback_count)
      : original_callback_(std::move(original_callback)),
        pending_callback_count_(wrapper_callback_count) {
    DCHECK(original_callback_);
    DCHECK_GE(pending_callback_count_, 0);
    if (!pending_callback_count_)
      std::move(original_callback_).Run(std::string() /* response */);
  }

  FirstNonEmptyMessageCallbackWrapper(
      const FirstNonEmptyMessageCallbackWrapper&) = delete;
  FirstNonEmptyMessageCallbackWrapper& operator=(
      const FirstNonEmptyMessageCallbackWrapper&) = delete;

  ~FirstNonEmptyMessageCallbackWrapper() {
    if (original_callback_) {
      // Not all responses were received before this instance is destroyed, so
      // run the callback with an error result here.
      std::move(original_callback_).Run(std::string() /* response */);
    }
  }

  void ProcessResponse(const std::string& response) {
    if (!original_callback_) {
      // The response was already passed in one of the previous invocations.
      return;
    }
    if (!response.empty()) {
      std::move(original_callback_).Run(response);
      return;
    }
    --pending_callback_count_;
    DCHECK_GE(pending_callback_count_, 0);
    if (pending_callback_count_ == 0) {
      // This is the last response and all responses have been empty, so pass
      // the empty response.
      std::move(original_callback_).Run(std::string() /* response */);
      return;
    }
  }

 private:
  base::OnceCallback<void(const std::string& response)> original_callback_;
  int pending_callback_count_;
};

void DeliverMessageToExtension(
    Profile* profile,
    const std::string& extension_id,
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)>
        send_response_callback) {
  const extensions::PortId port_id(base::UnguessableToken::Create(),
                                   1 /* port_number */, true /* is_opener */,
                                   extensions::SerializationFormat::kJson);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  auto native_message_host =
      std::make_unique<WilcoDtcSupportdDaemonOwnedMessageHost>(
          json_message, std::move(send_response_callback));
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(
          kWilcoDtcSupportdUiMessageHost),
      std::move(native_message_port), extension_id, GURL(),
      extensions::ChannelType::kNative, std::string() /* channel_name */);
}

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateExtensionOwnedWilcoDtcSupportdMessageHost(
    content::BrowserContext* browser_context) {
  return std::make_unique<WilcoDtcSupportdExtensionOwnedMessageHost>();
}

void DeliverWilcoDtcSupportdUiMessageToExtensions(
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)>
        send_response_callback) {
  if (json_message.size() > kWilcoDtcSupportdUiMessageMaxSize) {
    VLOG(1) << "Message received from the daemon is too big";
    return;
  }

  // Determine beforehand which extension instances should receive the event, in
  // order to be able to construct the wrapper callback with the needed counter.
  std::vector<std::pair<Profile*, std::string>> recipient_extensions;
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    for (const auto* extension_url : kWilcoDtcSupportdHostOrigins) {
      GURL url = GURL(extension_url);
      if (extensions::ExtensionRegistry::Get(profile)
              ->enabled_extensions()
              .GetExtensionOrAppByURL(url)) {
        std::string extension_id = url.host();
        recipient_extensions.emplace_back(profile, extension_id);
      }
    }
  }

  // Build the wrapper callback in order to call |send_response_callback| once
  // when:
  // * either the first non-empty response is received from one of the
  //   extensions;
  // * or requests to all extensions completed with no response.
  base::RepeatingCallback<void(const std::string& response)>
      first_non_empty_message_forwarding_callback = base::BindRepeating(
          &FirstNonEmptyMessageCallbackWrapper::ProcessResponse,
          base::Owned(new FirstNonEmptyMessageCallbackWrapper(
              std::move(send_response_callback),
              static_cast<int>(recipient_extensions.size()))));

  for (const auto& profile_and_extension : recipient_extensions) {
    DeliverMessageToExtension(profile_and_extension.first,
                              profile_and_extension.second, json_message,
                              first_non_empty_message_forwarding_callback);
  }
}

}  // namespace ash
