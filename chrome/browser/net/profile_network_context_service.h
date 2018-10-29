// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// KeyedService that initializes and provides access to the NetworkContexts for
// a Profile. This will eventually replace ProfileIOData.
class ProfileNetworkContextService : public KeyedService,
                                     public content_settings::Observer {
 public:
  explicit ProfileNetworkContextService(Profile* profile);
  ~ProfileNetworkContextService() override;

  // Creates a NetworkContext for the BrowserContext, using the specified
  // parameters. An empty |relative_partition_path| corresponds to the main
  // network context.
  //
  // Uses the network service if enabled. Otherwise creates one that will use
  // the IOThread's NetworkService. This may be called either before or after
  // SetUpProfileIODataNetworkContext.
  network::mojom::NetworkContextPtr CreateNetworkContext(
      bool in_memory,
      const base::FilePath& relative_partition_path);

  // Initializes |*network_context_params| to set up the ProfileIOData's
  // main URLRequestContext and |*network_context_request| to be one end of a
  // Mojo pipe to be bound to the NetworkContext for that URLRequestContext.
  // The caller will need to send these parameters to the IOThread's in-process
  // NetworkService.
  //
  // If the network service is disabled, CreateNetworkContext(), which is called
  // first, will return the other end of the pipe.  In this case, all requests
  // associated with this Profile will use the associated URLRequestContext
  // (either accessed through the StoragePartition's GetURLRequestContext() or
  // directly).
  //
  // If the network service is enabled, CreateNetworkContext() will instead
  // return a NetworkContext vended by the network service's NetworkService
  // (Instead of the IOThread's in-process one).  In this case, the
  // ProfileIOData's URLRequest context will be configured not to use on-disk
  // storage (so as not to conflict with the network service vended context),
  // and will only be used for legacy requests that use it directly.
  void SetUpProfileIODataNetworkContext(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextRequest* network_context_request,
      network::mojom::NetworkContextParamsPtr* network_context_params);

#if defined(OS_CHROMEOS)
  void UpdateTrustAnchors(const net::CertificateList& trust_anchors);
#endif

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

 private:
  // Checks |quic_allowed_|, and disables QUIC if needed.
  void DisableQuicIfNotAllowed();

  // Forwards changes to |pref_accept_language_| to the NetworkContext, after
  // formatting them as appropriate.
  void UpdateAcceptLanguage();

  // Forwards changes to |block_third_party_cookies_| to the NetworkContext.
  void UpdateBlockThirdPartyCookies();

  // Computes appropriate value of Accept-Language header based on
  // |pref_accept_language_|
  std::string ComputeAcceptLanguage() const;

  void UpdateReferrersEnabled();

  // Update the CTPolicy for the given NetworkContexts.
  void UpdateCTPolicyForContexts(
      const std::vector<network::mojom::NetworkContext*>& contexts);

  // Update the CTPolicy for the all of profiles_'s NetworkContexts.
  void UpdateCTPolicy();

  void ScheduleUpdateCTPolicy();

  // Creates parameters for the NetworkContext. Use |in_memory| instead of
  // |profile_->IsOffTheRecord()| because sometimes normal profiles want off the
  // record partitions (e.g. for webview tag).
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  Profile* const profile_;

  ProxyConfigMonitor proxy_config_monitor_;

  // The |in_memory| / |relative_partition_path| corresponding to the values
  // passed into CreateNetworkContext.
  using PartitionInfo = std::pair<bool, base::FilePath>;

  // These are the NetworkContext interfaces that use the ProfileIOData's
  // NetworkContexts. If the network service is disabled, ownership is passed to
  // StoragePartition when CreateNetworkContext is called.  Otherwise, retains
  // ownership, though nothing uses these after construction.
  std::map<PartitionInfo, network::mojom::NetworkContextPtr>
      profile_io_data_network_contexts_;

  // Request corresponding to |profile_io_data_main_network_context_|. Ownership
  // is passed to ProfileIOData when SetUpProfileIODataNetworkContext() is
  // called.
  std::map<PartitionInfo, network::mojom::NetworkContextRequest>
      profile_io_data_context_requests_;

  BooleanPrefMember quic_allowed_;
  StringPrefMember pref_accept_language_;
  BooleanPrefMember block_third_party_cookies_;
  BooleanPrefMember enable_referrers_;
  PrefChangeRegistrar pref_change_registrar_;

  // Used to post schedule CT policy updates
  base::OneShotTimer ct_policy_update_timer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNetworkContextService);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
