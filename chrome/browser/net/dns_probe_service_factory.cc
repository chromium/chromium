// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service_factory.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/secure_dns_mode.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace chrome_browser_net {

namespace {

// How long the DnsProbeService will cache the probe result for.
// If it's older than this and we get a probe request, the service expires it
// and starts a new probe.
const int kMaxResultAgeMs = 5000;

// The public DNS servers used by the DnsProbeService to verify internet
// connectivity.
const uint8_t kGooglePublicDns1[] = {8, 8, 8, 8};
const uint8_t kGooglePublicDns2[] = {8, 8, 4, 4};

network::mojom::NetworkContext* GetNetworkContextForProfile(
    content::BrowserContext* context) {
  return context->GetDefaultStoragePartition()->GetNetworkContext();
}

mojo::Remote<network::mojom::DnsConfigChangeManager>
GetDnsConfigChangeManager() {
  mojo::Remote<network::mojom::DnsConfigChangeManager>
      dns_config_change_manager_remote;
  content::GetNetworkService()->GetDnsConfigChangeManager(
      dns_config_change_manager_remote.BindNewPipeAndPassReceiver());
  return dns_config_change_manager_remote;
}

net::DnsConfigOverrides GoogleConfigOverrides() {
  net::DnsConfigOverrides overrides =
      net::DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  overrides.nameservers = std::vector<net::IPEndPoint>{
      net::IPEndPoint(net::IPAddress(kGooglePublicDns1),
                      net::dns_protocol::kDefaultPort),
      net::IPEndPoint(net::IPAddress(kGooglePublicDns2),
                      net::dns_protocol::kDefaultPort)};
  overrides.attempts = 1;
  overrides.secure_dns_mode = net::SecureDnsMode::kOff;
  return overrides;
}

class DnsProbeServiceImpl
    : public DnsProbeService,
      public network::mojom::DnsConfigChangeManagerClient {
 public:
  using DnsConfigChangeManagerGetter =
      DnsProbeServiceFactory::DnsConfigChangeManagerGetter;

  explicit DnsProbeServiceImpl(content::BrowserContext* context);
  DnsProbeServiceImpl(
      const network::NetworkContextGetter& network_context_getter,
      const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
      const base::TickClock* tick_clock);

  DnsProbeServiceImpl(const DnsProbeServiceImpl&) = delete;
  DnsProbeServiceImpl& operator=(const DnsProbeServiceImpl&) = delete;

  ~DnsProbeServiceImpl() override;

  // DnsProbeService implementation:
  void ProbeDns(DnsProbeService::ProbeCallback callback) override;
  net::DnsConfigOverrides GetCurrentConfigOverridesForTesting() override;

  // mojom::network::DnsConfigChangeManagerClient implementation:
  void OnDnsConfigChanged() override;

 private:
  enum State {
    STATE_NO_RESULT,
    STATE_PROBE_RUNNING,
    STATE_RESULT_CACHED,
  };

  // Create the current config runner using overrides that will mimic the
  // current secure DNS mode and pre-specified DoH servers.
  void SetUpCurrentConfigRunner();

  // Starts a probe (runs probes for both the current config and google config)
  void StartProbes();
  void OnProbeComplete();
  // Determine what error page should be shown given the results of the two
  // probe runners.
  error_page::DnsProbeStatus EvaluateResults(
      DnsProbeRunner::Result current_config_result,
      DnsProbeRunner::Result google_config_result);
  // Calls all |pending_callbacks_| with the |cached_result_|.
  void CallCallbacks();
  // Calls callback by posting a task to the same sequence. |pending_callbacks_|
  // must have exactly one element.
  void CallCallbackAsynchronously();
  // Clears a cached probe result.
  void ClearCachedResult();

  bool CachedResultIsExpired() const;

  void SetupDnsConfigChangeNotifications();
  void OnDnsConfigChangeManagerConnectionError();

  State state_;
  std::vector<DnsProbeService::ProbeCallback> pending_callbacks_;
  base::TimeTicks probe_start_time_;
  error_page::DnsProbeStatus cached_result_;

  network::NetworkContextGetter network_context_getter_;
  DnsConfigChangeManagerGetter dns_config_change_manager_getter_;
  mojo::Receiver<network::mojom::DnsConfigChangeManagerClient> receiver_{this};
  net::SecureDnsMode current_config_secure_dns_mode_ = net::SecureDnsMode::kOff;

  // DnsProbeRunners for the current DNS configuration and a Google DNS
  // configuration. Both runners will have the insecure async resolver enabled
  // regardless of the platform. This is out of necessity for the Google config
  // runner, where the nameservers are being changed. For the current config
  // runner, the insecure async resolver will only be used when the runner is
  // not operating in SECURE mode.
  std::unique_ptr<DnsProbeRunner> current_config_runner_;
  std::unique_ptr<DnsProbeRunner> google_config_runner_;

  // Time source for cache expiry.
  raw_ptr<const base::TickClock> tick_clock_;  // Not owned.

  SEQUENCE_CHECKER(sequence_checker_);
};

DnsProbeServiceImpl::DnsProbeServiceImpl(content::BrowserContext* context)
    : DnsProbeServiceImpl(
          base::BindRepeating(&GetNetworkContextForProfile, context),
          base::BindRepeating(&GetDnsConfigChangeManager),
          base::DefaultTickClock::GetInstance()) {}

DnsProbeServiceImpl::DnsProbeServiceImpl(
    const network::NetworkContextGetter& network_context_getter,
    const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
    const base::TickClock* tick_clock)
    : state_(STATE_NO_RESULT),
      network_context_getter_(network_context_getter),
      dns_config_change_manager_getter_(dns_config_change_manager_getter),
      tick_clock_(tick_clock) {
  SetupDnsConfigChangeNotifications();
  google_config_runner_ = std::make_unique<DnsProbeRunner>(
      GoogleConfigOverrides(), network_context_getter_);
  SetUpCurrentConfigRunner();
}

DnsProbeServiceImpl::~DnsProbeServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DnsProbeServiceImpl::ProbeDns(ProbeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_callbacks_.push_back(std::move(callback));

  if (CachedResultIsExpired())
    ClearCachedResult();

  switch (state_) {
    case STATE_NO_RESULT:
      StartProbes();
      break;
    case STATE_RESULT_CACHED:
      CallCallbackAsynchronously();
      break;
    case STATE_PROBE_RUNNING:
      // Do nothing; probe is already running, and will call the callback.
      break;
  }
}

net::DnsConfigOverrides
DnsProbeServiceImpl::GetCurrentConfigOverridesForTesting() {
  DCHECK(current_config_runner_);
  return current_config_runner_->GetConfigOverridesForTesting();
}

void DnsProbeServiceImpl::OnDnsConfigChanged() {
  SetUpCurrentConfigRunner();
  ClearCachedResult();
}

void DnsProbeServiceImpl::SetUpCurrentConfigRunner() {
  SecureDnsConfig secure_dns_config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              false /* force_check_parental_controls_for_automatic_mode */);

  current_config_secure_dns_mode_ = secure_dns_config.mode();

  net::DnsConfigOverrides current_config_overrides;
  current_config_overrides.search = std::vector<std::string>();
  current_config_overrides.attempts = 1;

  if (current_config_secure_dns_mode_ == net::SecureDnsMode::kSecure) {
    if (!secure_dns_config.doh_servers().servers().empty()) {
      current_config_overrides.dns_over_https_config =
          secure_dns_config.doh_servers();
    }
    current_config_overrides.secure_dns_mode = net::SecureDnsMode::kSecure;
  } else {
    // A DNS error that occurred in automatic mode must have had an insecure
    // DNS failure. For efficiency, probe queries in this case can just be
    // issued in OFF mode.
    current_config_overrides.secure_dns_mode = net::SecureDnsMode::kOff;
  }

  current_config_runner_ = std::make_unique<DnsProbeRunner>(
      current_config_overrides, network_context_getter_);
}

void DnsProbeServiceImpl::StartProbes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_NO_RESULT, state_);

  DCHECK(!current_config_runner_->IsRunning());
  DCHECK(!google_config_runner_->IsRunning());

  // Unretained safe because the callback will not be run if the DnsProbeRunner
  // is destroyed.
  current_config_runner_->RunProbe(base::BindOnce(
      &DnsProbeServiceImpl::OnProbeComplete, base::Unretained(this)));
  google_config_runner_->RunProbe(base::BindOnce(
      &DnsProbeServiceImpl::OnProbeComplete, base::Unretained(this)));
  probe_start_time_ = tick_clock_->NowTicks();
  state_ = STATE_PROBE_RUNNING;

  DCHECK(current_config_runner_->IsRunning());
  DCHECK(google_config_runner_->IsRunning());
}

void DnsProbeServiceImpl::OnProbeComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);
  DCHECK(!current_config_runner_->IsRunning() ||
         !google_config_runner_->IsRunning());

  if (current_config_runner_->IsRunning() || google_config_runner_->IsRunning())
    return;

  cached_result_ = EvaluateResults(current_config_runner_->result(),
                                   google_config_runner_->result());
  state_ = STATE_RESULT_CACHED;

  CallCallbacks();
}

error_page::DnsProbeStatus DnsProbeServiceImpl::EvaluateResults(
    DnsProbeRunner::Result current_config_result,
    DnsProbeRunner::Result google_config_result) {
  // If the current DNS config is working, assume the domain doesn't exist.
  if (current_config_result == DnsProbeRunner::CORRECT)
    return error_page::DNS_PROBE_FINISHED_NXDOMAIN;

  // If the current DNS config is unknown (e.g. on Android), but Google DNS is
  // reachable, assume the domain doesn't exist.
  if (current_config_result == DnsProbeRunner::UNKNOWN &&
      google_config_result == DnsProbeRunner::CORRECT) {
    return error_page::DNS_PROBE_FINISHED_NXDOMAIN;
  }

  // If the current DNS config is not working but Google DNS is, assume the DNS
  // config is bad (or perhaps the DNS servers are down or broken). If the
  // current DNS config is in secure mode, return an error indicating that this
  // is a secure DNS config issue.
  if (google_config_result == DnsProbeRunner::CORRECT) {
    return (current_config_secure_dns_mode_ == net::SecureDnsMode::kSecure)
               ? error_page::DNS_PROBE_FINISHED_BAD_SECURE_CONFIG
               : error_page::DNS_PROBE_FINISHED_BAD_CONFIG;
  }

  // If the current DNS config is not working and Google DNS is unreachable,
  // assume the internet connection is down (note that current DNS may be a
  // router on the LAN, so it may be reachable but returning errors.)
  if (google_config_result == DnsProbeRunner::UNREACHABLE)
    return error_page::DNS_PROBE_FINISHED_NO_INTERNET;

  // Otherwise: the current DNS config is not working and Google DNS is
  // responding but with errors or incorrect results.  This is an awkward case;
  // an invasive captive portal or a restrictive firewall may be intercepting
  // or rewriting DNS traffic, or the public server may itself be failing or
  // down.
  return error_page::DNS_PROBE_FINISHED_INCONCLUSIVE;
}

void DnsProbeServiceImpl::CallCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_RESULT_CACHED, state_);
  DCHECK(error_page::DnsProbeStatusIsFinished(cached_result_));
  DCHECK(!pending_callbacks_.empty());

  std::vector<ProbeCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  for (std::vector<ProbeCallback>::iterator i = callbacks.begin();
       i != callbacks.end(); ++i) {
    std::move(*i).Run(cached_result_);
  }
}

void DnsProbeServiceImpl::CallCallbackAsynchronously() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_RESULT_CACHED, state_);
  DCHECK(error_page::DnsProbeStatusIsFinished(cached_result_));
  DCHECK_EQ(1U, pending_callbacks_.size());

  std::vector<ProbeCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callbacks.front()), cached_result_));
}

void DnsProbeServiceImpl::ClearCachedResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == STATE_RESULT_CACHED) {
    state_ = STATE_NO_RESULT;
    cached_result_ = error_page::DNS_PROBE_MAX;
  }
}

bool DnsProbeServiceImpl::CachedResultIsExpired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STATE_RESULT_CACHED)
    return false;

  const base::TimeDelta kMaxResultAge = base::Milliseconds(kMaxResultAgeMs);
  return tick_clock_->NowTicks() - probe_start_time_ > kMaxResultAge;
}

void DnsProbeServiceImpl::SetupDnsConfigChangeNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dns_config_change_manager_getter_.Run()->RequestNotifications(
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &DnsProbeServiceImpl::OnDnsConfigChangeManagerConnectionError,
      base::Unretained(this)));
}

void DnsProbeServiceImpl::OnDnsConfigChangeManagerConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clear the cache, since the configuration may have changed while not
  // getting notifications.
  ClearCachedResult();

  receiver_.reset();

  SetupDnsConfigChangeNotifications();
}

}  // namespace

DnsProbeService* DnsProbeServiceFactory::GetForContext(
    content::BrowserContext* browser_context) {
  return static_cast<DnsProbeService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

DnsProbeServiceFactory* DnsProbeServiceFactory::GetInstance() {
  static base::NoDestructor<DnsProbeServiceFactory> instance;
  return instance.get();
}

DnsProbeServiceFactory::DnsProbeServiceFactory()
    : ProfileKeyedServiceFactory(
          "DnsProbeService",
          // Create separate service for incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

DnsProbeServiceFactory::~DnsProbeServiceFactory() = default;

std::unique_ptr<KeyedService>
DnsProbeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DnsProbeServiceImpl>(context);
}

// static
std::unique_ptr<DnsProbeService> DnsProbeServiceFactory::CreateForTesting(
    const network::NetworkContextGetter& network_context_getter,
    const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
    const base::TickClock* tick_clock) {
  return std::make_unique<DnsProbeServiceImpl>(
      network_context_getter, dns_config_change_manager_getter, tick_clock);
}

}  // namespace chrome_browser_net
