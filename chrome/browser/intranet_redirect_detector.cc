// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intranet_redirect_detector.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/browser/intranet_redirector_state.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"

// TODO(crbug.com/40966307): Write test to verify we handle the policy toggling.
IntranetRedirectDetector::IntranetRedirectDetector()
    : redirect_origin_(g_browser_process->local_state()->GetString(
          prefs::kLastKnownIntranetRedirectOrigin)) {
  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay seven seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static constexpr base::TimeDelta kStartFetchDelay = base::Seconds(7);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IntranetRedirectDetector::FinishSleep,
                     weak_ptr_factory_.GetWeakPtr()),
      kStartFetchDelay);

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  SetupDnsConfigClient();
}

IntranetRedirectDetector::~IntranetRedirectDetector() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

// static
GURL IntranetRedirectDetector::RedirectOrigin() {
  const IntranetRedirectDetector* const detector =
      g_browser_process->intranet_redirect_detector();
  return detector ? detector->redirect_origin_ : GURL();
}

// static
void IntranetRedirectDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLastKnownIntranetRedirectOrigin,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDNSInterceptionChecksEnabled, true);
  registry->RegisterIntegerPref(omnibox::kIntranetRedirectBehavior, 0);
}

void IntranetRedirectDetector::Restart() {
  if (!IsEnabledByPolicy()) {
    if (redirect_origin_.is_valid()) {
      g_browser_process->local_state()->SetString(
          prefs::kLastKnownIntranetRedirectOrigin, std::string());
    }
    redirect_origin_ = GURL();
    return;
  }
  // If a request is already scheduled, do not scheduled yet another one.
  if (in_sleep_)
    return;

  // Since presumably many programs open connections after network changes,
  // delay this a little bit.
  in_sleep_ = true;
  static constexpr base::TimeDelta kRestartDelay = base::Seconds(1);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IntranetRedirectDetector::FinishSleep,
                     weak_ptr_factory_.GetWeakPtr()),
      kRestartDelay);
}

void IntranetRedirectDetector::FinishSleep() {
  in_sleep_ = false;
  if (!IsEnabledByPolicy()) {
    if (redirect_origin_.is_valid()) {
      g_browser_process->local_state()->SetString(
          prefs::kLastKnownIntranetRedirectOrigin, std::string());
    }
    redirect_origin_ = GURL();
    return;
  }

  // If another fetch operation is still running, cancel it.
  simple_loaders_.clear();
  resulting_origins_.clear();

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBackgroundNetworking))
    return;

  DCHECK(simple_loaders_.empty() && resulting_origins_.empty());

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("intranet_redirect_detector", R"(
        semantics {
          sender: "Intranet Redirect Detector"
          description:
            "This component sends requests to three randomly generated, and "
            "thus likely nonexistent, hostnames.  If at least two redirect to "
            "the same hostname, this suggests the ISP is hijacking NXDOMAIN, "
            "and the omnibox should treat similar redirected navigations as "
            "'failed' when deciding whether to prompt the user with a 'did you "
            "mean to navigate' infobar for certain search inputs."
          trigger: "On startup and when IP address of the computer changes."
          data: "None, this is just an empty request."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
              "Not implemented, considered not useful."
        })");

  // Start three loaders on random hostnames.
  for (size_t i = 0; i < 3; ++i) {
    std::string url_string("http://");
    // We generate a random hostname with between 7 and 15 characters.
    const int num_chars = base::RandInt(7, 15);
    for (int j = 0; j < num_chars; ++j)
      url_string += ('a' + base::RandInt(0, 'z' - 'a'));
    GURL random_url(url_string + '/');

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = random_url;
    resource_request->method = "HEAD";
    // We don't want these fetches to affect existing state in the profile.
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    network::mojom::URLLoaderFactory* loader_factory =
        g_browser_process->system_network_context_manager()
            ->GetURLLoaderFactory();
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         traffic_annotation);
    network::SimpleURLLoader* simple_loader_ptr = simple_loader.get();
    simple_loader->DownloadToString(
        loader_factory,
        base::BindOnce(&IntranetRedirectDetector::OnSimpleLoaderComplete,
                       base::Unretained(this), simple_loader_ptr),
        /*max_body_size=*/1);
    simple_loaders_[simple_loader_ptr] = std::move(simple_loader);
  }
}

void IntranetRedirectDetector::OnSimpleLoaderComplete(
    network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  // Delete the loader on this function's exit.
  auto it = simple_loaders_.find(source);
  CHECK(it != simple_loaders_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      std::move(it->second);
  simple_loaders_.erase(it);

  // If any two loaders result in the same domain/host, we set the redirect
  // origin to that; otherwise we set it to nothing.
  if (response_body) {
    DCHECK(source->GetFinalURL().is_valid());
    GURL origin(source->GetFinalURL().DeprecatedGetOriginAsURL());
    if (resulting_origins_.empty()) {
      resulting_origins_.push_back(origin);
      return;
    }
    if (net::registry_controlled_domains::SameDomainOrHost(
        resulting_origins_.front(),
        origin,
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)) {
      redirect_origin_ = origin;
      if (!simple_loaders_.empty()) {
        // Cancel remaining loader, we don't need it.
        DCHECK(simple_loaders_.size() == 1);
        simple_loaders_.clear();
      }
    }
    if (resulting_origins_.size() == 1) {
      resulting_origins_.push_back(origin);
      return;
    }
    DCHECK(resulting_origins_.size() == 2);
    const bool same_domain_or_host =
        net::registry_controlled_domains::SameDomainOrHost(
            resulting_origins_.back(),
            origin,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    redirect_origin_ = same_domain_or_host ? origin : GURL();
  } else {
    if (resulting_origins_.empty() || (resulting_origins_.size() == 1 &&
                                       resulting_origins_.front().is_valid())) {
      resulting_origins_.push_back(GURL());
      return;
    }
    redirect_origin_ = GURL();
  }

  g_browser_process->local_state()->SetString(
      prefs::kLastKnownIntranetRedirectOrigin, redirect_origin_.is_valid() ?
          redirect_origin_.spec() : std::string());
}

void IntranetRedirectDetector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type != network::mojom::ConnectionType::CONNECTION_NONE)
    Restart();
}

void IntranetRedirectDetector::OnDnsConfigChanged() {
  Restart();
}

void IntranetRedirectDetector::SetupDnsConfigClient() {
  DCHECK(!dns_config_client_receiver_.is_bound());

  mojo::Remote<network::mojom::DnsConfigChangeManager> manager_remote;
  content::GetNetworkService()->GetDnsConfigChangeManager(
      manager_remote.BindNewPipeAndPassReceiver());
  manager_remote->RequestNotifications(
      dns_config_client_receiver_.BindNewPipeAndPassRemote());
  dns_config_client_receiver_.set_disconnect_handler(base::BindOnce(
      &IntranetRedirectDetector::OnDnsConfigClientConnectionError,
      base::Unretained(this)));
}

void IntranetRedirectDetector::OnDnsConfigClientConnectionError() {
  dns_config_client_receiver_.reset();
  SetupDnsConfigClient();
}

bool IntranetRedirectDetector::IsEnabledByPolicy() {
  // The InterceptionChecksBehavior pref and the older
  // DNSInterceptionChecksEnabled policy should each be able to disable
  // interception checks. Therefore, we enable the redirect detector iff allowed
  // by both policies.

  // Check IntranetRedirectorBehavior pref.
  auto behavior =
      omnibox::GetInterceptionChecksBehavior(g_browser_process->local_state());
  if (behavior == omnibox::IntranetRedirectorBehavior::DISABLE_FEATURE ||
      behavior == omnibox::IntranetRedirectorBehavior::
                      DISABLE_INTERCEPTION_CHECKS_ENABLE_INFOBARS) {
    return false;
  }

  // Consult previous DNSInterceptionChecksEnabled policy.
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kDNSInterceptionChecksEnabled))
    return false;

  return true;
}
