// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/proxy_config_monitor.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#endif

using content::BrowserThread;

ProxyConfigMonitor::ProxyConfigMonitor(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  profile_ = profile;
#endif

// If this is the ChromeOS sign-in or lock screen profile, just create the
// tracker from global state.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsSigninProfile(profile) ||
      ash::ProfileHelper::IsLockScreenProfile(profile)) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
            g_browser_process->local_state());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            profile->GetPrefs(), g_browser_process->local_state());
  }

  proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get(), profile);

  proxy_config_service_->AddObserver(this);
}

ProxyConfigMonitor::ProxyConfigMonitor(PrefService* local_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  pref_proxy_config_tracker_ =
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state);

  proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get(), nullptr);
  proxy_config_service_->AddObserver(this);
}

ProxyConfigMonitor::~ProxyConfigMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  proxy_config_service_->RemoveObserver(this);
  pref_proxy_config_tracker_->DetachFromPrefService();
}

void ProxyConfigMonitor::AddToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  mojo::PendingRemote<network::mojom::ProxyConfigClient> proxy_config_client;
  network_context_params->proxy_config_client_receiver =
      proxy_config_client.InitWithNewPipeAndPassReceiver();
  proxy_config_client_set_.Add(std::move(proxy_config_client));

  if (proxy_config_service_->UsesPolling()) {
    poller_receiver_set_.Add(this,
                             network_context_params->proxy_config_poller_client
                                 .InitWithNewPipeAndPassReceiver());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  error_receiver_set_.Add(this, network_context_params->proxy_error_client
                                    .InitWithNewPipeAndPassReceiver());
#endif

  net::ProxyConfigWithAnnotation proxy_config;
  net::ProxyConfigService::ConfigAvailability availability =
      proxy_config_service_->GetLatestProxyConfig(&proxy_config);
  if (availability != net::ProxyConfigService::CONFIG_PENDING)
    network_context_params->initial_proxy_config = proxy_config;
}

void ProxyConfigMonitor::FlushForTesting() {
  proxy_config_client_set_.FlushForTesting();
}

void ProxyConfigMonitor::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    net::ProxyConfigService::ConfigAvailability availability) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  for (const auto& proxy_config_client : proxy_config_client_set_) {
    switch (availability) {
      case net::ProxyConfigService::CONFIG_VALID:
        proxy_config_client->OnProxyConfigUpdated(config);
        break;
      case net::ProxyConfigService::CONFIG_UNSET:
        proxy_config_client->OnProxyConfigUpdated(
            net::ProxyConfigWithAnnotation::CreateDirect());
        break;
      case net::ProxyConfigService::CONFIG_PENDING:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

void ProxyConfigMonitor::OnLazyProxyConfigPoll() {
  proxy_config_service_->OnLazyPoll();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ProxyConfigMonitor::OnPACScriptError(int32_t line_number,
                                          const std::string& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  extensions::ProxyEventRouter::GetInstance()->OnPACScriptError(
      profile_, line_number, base::UTF8ToUTF16(details));
}

void ProxyConfigMonitor::OnRequestMaybeFailedDueToProxySettings(
    int32_t net_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  if (net_error >= 0) {
    // If the error is obviously wrong, don't dispatch it to extensions. If the
    // PAC executor process is compromised, then |net_error| could be attacker
    // controlled.
    return;
  }

  extensions::ProxyEventRouter::GetInstance()->OnProxyError(profile_,
                                                            net_error);
}
#endif
