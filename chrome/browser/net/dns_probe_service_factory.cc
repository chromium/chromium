// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service_factory.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_protocol.h"
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

error_page::DnsProbeStatus EvaluateResults(
    DnsProbeRunner::Result system_result,
    DnsProbeRunner::Result public_result) {
  // If the system DNS is working, assume the domain doesn't exist.
  if (system_result == DnsProbeRunner::CORRECT)
    return error_page::DNS_PROBE_FINISHED_NXDOMAIN;

  // If the system DNS is unknown (e.g. on Android), but the public server is
  // reachable, assume the domain doesn't exist.
  if (system_result == DnsProbeRunner::UNKNOWN &&
      public_result == DnsProbeRunner::CORRECT) {
    return error_page::DNS_PROBE_FINISHED_NXDOMAIN;
  }

  // If the system DNS is not working but another public server is, assume the
  // DNS config is bad (or perhaps the DNS servers are down or broken).
  if (public_result == DnsProbeRunner::CORRECT)
    return error_page::DNS_PROBE_FINISHED_BAD_CONFIG;

  // If the system DNS is not working and another public server is unreachable,
  // assume the internet connection is down (note that system DNS may be a
  // router on the LAN, so it may be reachable but returning errors.)
  if (public_result == DnsProbeRunner::UNREACHABLE)
    return error_page::DNS_PROBE_FINISHED_NO_INTERNET;

  // Otherwise: the system DNS is not working and another public server is
  // responding but with errors or incorrect results.  This is an awkward case;
  // an invasive captive portal or a restrictive firewall may be intercepting
  // or rewriting DNS traffic, or the public server may itself be failing or
  // down.
  return error_page::DNS_PROBE_FINISHED_INCONCLUSIVE;
}

void HistogramProbe(error_page::DnsProbeStatus status,
                    base::TimeDelta elapsed) {
  DCHECK(error_page::DnsProbeStatusIsFinished(status));

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ProbeResult", status,
                            error_page::DNS_PROBE_MAX);
  UMA_HISTOGRAM_MEDIUM_TIMES("DnsProbe.ProbeDuration2", elapsed);
}

network::mojom::NetworkContext* GetNetworkContextForProfile(
    content::BrowserContext* context) {
  return content::BrowserContext::GetDefaultStoragePartition(context)
      ->GetNetworkContext();
}

mojo::Remote<network::mojom::DnsConfigChangeManager>
GetDnsConfigChangeManager() {
  mojo::Remote<network::mojom::DnsConfigChangeManager>
      dns_config_change_manager_remote;
  content::GetNetworkService()->GetDnsConfigChangeManager(
      dns_config_change_manager_remote.BindNewPipeAndPassReceiver());
  return dns_config_change_manager_remote;
}

net::DnsConfigOverrides SystemOverrides() {
  net::DnsConfigOverrides overrides;
  overrides.search = {};
  overrides.attempts = 1;
  overrides.randomize_ports = false;
  return overrides;
}

net::DnsConfigOverrides PublicOverrides() {
  net::DnsConfigOverrides overrides =
      net::DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  overrides.nameservers = std::vector<net::IPEndPoint>{
      net::IPEndPoint(net::IPAddress(kGooglePublicDns1),
                      net::dns_protocol::kDefaultPort),
      net::IPEndPoint(net::IPAddress(kGooglePublicDns2),
                      net::dns_protocol::kDefaultPort)};
  overrides.attempts = 1;
  overrides.randomize_ports = false;
  return overrides;
}

class DnsProbeServiceImpl
    : public DnsProbeService,
      public network::mojom::DnsConfigChangeManagerClient {
 public:
  using NetworkContextGetter = DnsProbeServiceFactory::NetworkContextGetter;
  using DnsConfigChangeManagerGetter =
      DnsProbeServiceFactory::DnsConfigChangeManagerGetter;

  explicit DnsProbeServiceImpl(content::BrowserContext* context);
  DnsProbeServiceImpl(
      const NetworkContextGetter& network_context_getter,
      const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
      const base::TickClock* tick_clock);
  ~DnsProbeServiceImpl() override;

  // DnsProbeService implementation:
  void ProbeDns(DnsProbeService::ProbeCallback callback) override;

  // mojom::network::DnsConfigChangeManagerClient implementation:
  void OnDnsConfigChanged() override;

 private:
  enum State {
    STATE_NO_RESULT,
    STATE_PROBE_RUNNING,
    STATE_RESULT_CACHED,
  };

  // Starts a probe (runs system and public probes).
  void StartProbes();
  void OnProbeComplete();
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

  NetworkContextGetter network_context_getter_;
  DnsConfigChangeManagerGetter dns_config_change_manager_getter_;
  mojo::Receiver<network::mojom::DnsConfigChangeManagerClient> receiver_{this};
  // DnsProbeRunners for the system DNS configuration and a public DNS server.
  DnsProbeRunner system_runner_;
  DnsProbeRunner public_runner_;

  // Time source for cache expiry.
  const base::TickClock* tick_clock_;  // Not owned.

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DnsProbeServiceImpl);
};

DnsProbeServiceImpl::DnsProbeServiceImpl(content::BrowserContext* context)
    : DnsProbeServiceImpl(
          base::BindRepeating(&GetNetworkContextForProfile, context),
          base::BindRepeating(&GetDnsConfigChangeManager),
          base::DefaultTickClock::GetInstance()) {}

DnsProbeServiceImpl::DnsProbeServiceImpl(
    const NetworkContextGetter& network_context_getter,
    const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
    const base::TickClock* tick_clock)
    : state_(STATE_NO_RESULT),
      network_context_getter_(network_context_getter),
      dns_config_change_manager_getter_(dns_config_change_manager_getter),
      system_runner_(SystemOverrides(), network_context_getter),
      public_runner_(PublicOverrides(), network_context_getter),
      tick_clock_(tick_clock) {
  SetupDnsConfigChangeNotifications();
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

void DnsProbeServiceImpl::OnDnsConfigChanged() {
  ClearCachedResult();
}

void DnsProbeServiceImpl::StartProbes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_NO_RESULT, state_);

  DCHECK(!system_runner_.IsRunning());
  DCHECK(!public_runner_.IsRunning());

  // Unretained safe because the callback will not be run if the DnsProbeRunner
  // is destroyed.
  system_runner_.RunProbe(base::BindOnce(&DnsProbeServiceImpl::OnProbeComplete,
                                         base::Unretained(this)));
  public_runner_.RunProbe(base::BindOnce(&DnsProbeServiceImpl::OnProbeComplete,
                                         base::Unretained(this)));
  probe_start_time_ = tick_clock_->NowTicks();
  state_ = STATE_PROBE_RUNNING;

  DCHECK(system_runner_.IsRunning());
  DCHECK(public_runner_.IsRunning());
}

void DnsProbeServiceImpl::OnProbeComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(STATE_PROBE_RUNNING, state_);
  DCHECK(!system_runner_.IsRunning() || !public_runner_.IsRunning());

  if (system_runner_.IsRunning() || public_runner_.IsRunning())
    return;

  cached_result_ =
      EvaluateResults(system_runner_.result(), public_runner_.result());
  state_ = STATE_RESULT_CACHED;

  HistogramProbe(cached_result_, tick_clock_->NowTicks() - probe_start_time_);

  CallCallbacks();
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

  base::SequencedTaskRunnerHandle::Get()->PostTask(
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

  const base::TimeDelta kMaxResultAge =
      base::TimeDelta::FromMilliseconds(kMaxResultAgeMs);
  return tick_clock_->NowTicks() - probe_start_time_ > kMaxResultAge;
}

void DnsProbeServiceImpl::SetupDnsConfigChangeNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dns_config_change_manager_getter_.Run()->RequestNotifications(
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindRepeating(
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
  return base::Singleton<DnsProbeServiceFactory>::get();
}

DnsProbeServiceFactory::DnsProbeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DnsProbeService",
          BrowserContextDependencyManager::GetInstance()) {}

DnsProbeServiceFactory::~DnsProbeServiceFactory() {}

KeyedService* DnsProbeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DnsProbeServiceImpl(context);
}

content::BrowserContext* DnsProbeServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create separate service for incognito profiles.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

// static
std::unique_ptr<DnsProbeService> DnsProbeServiceFactory::CreateForTesting(
    const NetworkContextGetter& network_context_getter,
    const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
    const base::TickClock* tick_clock) {
  return std::make_unique<DnsProbeServiceImpl>(
      network_context_getter, dns_config_change_manager_getter, tick_clock);
}

}  // namespace chrome_browser_net
