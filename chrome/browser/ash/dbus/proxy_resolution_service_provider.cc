// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/proxy_resolution_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The proxy result to return when resolution fails.
// It is up to the D-Bus caller to determine how to interpret this in the
// case of errors, but DIRECT is usually a good fallback.
//
// TODO(eroman): This doesn't properly convey the semantics of
// ERR_MANDATORY_PROXY_CONFIGURATION_FAILED. For this error, consumers
// should fail the entire network request rather than falling back to
// DIRECT connections, to behave the same as the browser.
const char kProxyInfoOnFailure[] = "DIRECT";

class ProxyLookupRequest : public network::mojom::ProxyLookupClient {
 public:
  // Sends a proxy lookup request to the Network Service and invokes
  // |notify_callback| on completion. Caller should not manage the memory of
  // |this|, as it will delete itself on completion.
  ProxyLookupRequest(
      network::mojom::NetworkContext* network_context,
      const GURL& source_url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      ProxyResolutionServiceProvider::NotifyCallback notify_callback,
      chromeos::SystemProxyOverride system_proxy_override)
      : notify_callback_(std::move(notify_callback)),
        system_proxy_override_(system_proxy_override) {
    mojo::PendingRemote<network::mojom::ProxyLookupClient> proxy_lookup_client =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&ProxyLookupRequest::OnProxyLookupComplete,
                       base::Unretained(this), net::ERR_ABORTED, std::nullopt));

    network_context->LookUpProxyForURL(source_url, network_anonymization_key,
                                       std::move(proxy_lookup_client));
  }

  ProxyLookupRequest(const ProxyLookupRequest&) = delete;
  ProxyLookupRequest& operator=(const ProxyLookupRequest&) = delete;

  ~ProxyLookupRequest() override = default;

  void OnProxyLookupComplete(
      int32_t net_error,
      const std::optional<net::ProxyInfo>& proxy_info) override {
    DCHECK_EQ(net_error == net::OK, proxy_info.has_value());

    std::string error;
    std::string result;

    if (!proxy_info) {
      error = net::ErrorToString(net_error);
      result = kProxyInfoOnFailure;
    } else if (proxy_info->ContainsMultiProxyChain()) {
      // Multi-proxy chains cannot be represented as a PAC string.
      error = net::ErrorToString(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED);
      result = kProxyInfoOnFailure;
    } else {
      result = proxy_info->ToPacString();
      if (!proxy_info->is_empty() && !proxy_info->is_direct() &&
          proxy_info->proxy_chain()
              .GetProxyServer(/*chain_index=*/0)
              .is_http()) {
        AppendSystemProxyIfActive(&result);
      }
    }
    receiver_.reset();
    std::move(notify_callback_).Run(error, result);
    delete this;
  }

 private:
  // Appends the System-proxy address, if active, to the list of existing
  // proxies, which can still be used by system services as a fallback if the
  // local proxy connection fails. System-proxy itself does proxy resolution
  // trough the same Chrome proxy resolution service to connect to the
  // remote proxy server. The availability of this feature is controlled by the
  // |SystemProxySettings| policy and the feature flag
  // `features::kSystemProxyForSystemServices`.
  void AppendSystemProxyIfActive(std::string* pac_proxy_list) {
    SystemProxyManager* system_proxy_manager = SystemProxyManager::Get();
    // |system_proxy_manager| may be missing in tests.
    if (!system_proxy_manager)
      return;

    std::string system_proxy_pac =
        system_proxy_manager->SystemServicesProxyPacString(
            system_proxy_override_);
    if (system_proxy_pac.empty())
      return;

    *pac_proxy_list = base::StringPrintf("%s; %s", system_proxy_pac.c_str(),
                                         pac_proxy_list->c_str());
  }

  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};
  ProxyResolutionServiceProvider::NotifyCallback notify_callback_;
  chromeos::SystemProxyOverride system_proxy_override_;
};

}  // namespace

ProxyResolutionServiceProvider::ProxyResolutionServiceProvider()
    : origin_thread_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      network_anonymization_key_(
          net::NetworkAnonymizationKey::CreateTransient()) {}

ProxyResolutionServiceProvider::~ProxyResolutionServiceProvider() {
  DCHECK(OnOriginThread());
}

void ProxyResolutionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(OnOriginThread());
  exported_object_ = exported_object;
  VLOG(1) << "ProxyResolutionServiceProvider started";
  exported_object_->ExportMethod(
      chromeos::kNetworkProxyServiceInterface,
      chromeos::kNetworkProxyServiceResolveProxyMethod,
      base::BindRepeating(&ProxyResolutionServiceProvider::DbusResolveProxy,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ProxyResolutionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ProxyResolutionServiceProvider::OnOriginThread() {
  return origin_thread_->BelongsToCurrentThread();
}

void ProxyResolutionServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (success)
    VLOG(1) << "Method exported: " << interface_name << "." << method_name;
  else
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void ProxyResolutionServiceProvider::DbusResolveProxy(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(OnOriginThread());

  VLOG(1) << "Handling method call: " << method_call->ToString();
  dbus::MessageReader reader(method_call);
  std::string source_url;
  if (!reader.PopString(&source_url)) {
    LOG(ERROR) << "Method call lacks source URL: " << method_call->ToString();
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "No source URL string arg"));
    return;
  }

  // Read `chromeos::SystemProxyOverride` option.
  int int_state = 0;
  if (!reader.PopInt32(&int_state)) {
    VLOG(1) << "No SystemProxyOverride option specified.";
  }
  chromeos::SystemProxyOverride system_proxy_override;

  if (int_state < 0 || int_state > chromeos::SystemProxyOverride::kOptOut) {
    LOG(ERROR) << "Invalid value for SystemProxyOverride " << int_state;
    // Fallback to default state,
    system_proxy_override = chromeos::SystemProxyOverride::kDefault;
  } else {
    system_proxy_override =
        static_cast<chromeos::SystemProxyOverride>(int_state);
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  NotifyCallback notify_dbus_callback =
      base::BindOnce(&ProxyResolutionServiceProvider::NotifyProxyResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response),
                     std::move(response_sender));

  ResolveProxyInternal(source_url, std::move(notify_dbus_callback),
                       system_proxy_override);
}

void ProxyResolutionServiceProvider::ResolveProxyInternal(
    const std::string& source_url,
    NotifyCallback callback,
    chromeos::SystemProxyOverride system_proxy_override) {
  auto* network_context = GetNetworkContext();

  if (!network_context) {
    std::move(callback).Run("No NetworkContext", kProxyInfoOnFailure);
    return;
  }

  GURL url(source_url);
  if (!url.is_valid()) {
    std::move(callback).Run("Invalid URL", kProxyInfoOnFailure);
    return;
  }

  VLOG(1) << "Starting network proxy resolution for " << url;
  new ProxyLookupRequest(network_context, url, network_anonymization_key_,
                         std::move(callback), system_proxy_override);
}

void ProxyResolutionServiceProvider::NotifyProxyResolved(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::string& error,
    const std::string& pac_string) {
  DCHECK(OnOriginThread());

  // Reply to the original D-Bus method call.
  dbus::MessageWriter writer(response.get());
  writer.AppendString(pac_string);
  writer.AppendString(error);
  std::move(response_sender).Run(std::move(response));
}

network::mojom::NetworkContext*
ProxyResolutionServiceProvider::GetNetworkContext() {
  if (use_network_context_for_test_)
    return network_context_for_test_;

  // TODO(eroman): Instead of retrieving the profile globally (which could be in
  // a variety of states during startup/shutdown), pass the BrowserContext in as
  // a dependency.

  // Can be the profile of the primary user logged in the session or the profile
  // associated with the sign-in screen.
  Profile* profile = nullptr;
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    profile = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
            primary_user));
  }

  if (!profile) {
    profile = ProfileManager::GetActiveUserProfile();
    if (!profile || !ash::IsSigninBrowserContext(profile)) {
      return nullptr;
    }
  }

  auto* storage_partition = profile->GetDefaultStoragePartition();

  if (!storage_partition)
    return nullptr;

  return storage_partition->GetNetworkContext();
}

}  // namespace ash
