// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/pref_names.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/prefs/pref_service.h"

namespace media_router {

constexpr char kLoggerComponent[] = "CastMediaSinkService";

CastMediaSinkService::CastMediaSinkService()
    : impl_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

CastMediaSinkService::~CastMediaSinkService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dns_sd_registry_) {
    dns_sd_registry_->UnregisterDnsSdListener(kCastServiceType);
    dns_sd_registry_->RemoveObserver(this);
    dns_sd_registry_ = nullptr;
  }
}

void CastMediaSinkService::Initialize(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    base::RepeatingClosure discovery_permission_rejected_cb,
    MediaSinkServiceBase* dial_media_sink_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!impl_);
  discovery_permission_rejected_cb_ = discovery_permission_rejected_cb;

  // |sinks_discovered_cb| should only be invoked on the current sequence.
  // We wrap |sinks_discovered_cb| in a member function bound with WeakPtr to
  // ensure it will only be invoked while |this| is still valid.
  // TODO(imcheng): Simplify this by only using observers instead of callback.
  // This would require us to move the timer logic from MediaSinkServiceBase up
  // to DualMediaSinkService, but will allow us to remove MediaSinkServiceBase.
  impl_ =
      CreateImpl(base::BindRepeating(
                     &RunSinksDiscoveredCallbackOnSequence,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     base::BindRepeating(
                         &CastMediaSinkService::RunSinksDiscoveredCallback,
                         weak_ptr_factory_.GetWeakPtr(), sinks_discovered_cb)),
                 dial_media_sink_service);
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::Start,
                                base::Unretained(impl_.get())));

#if BUILDFLAG(IS_WIN)
  bool should_delay_discovery = true;
#else
  bool should_delay_discovery =
      base::FeatureList::IsEnabled(media_router::kDelayMediaSinkDiscovery);
#endif

  if (should_delay_discovery) {
    LoggerList::GetInstance()->Log(
        LoggerImpl::Severity::kInfo, mojom::LogCategory::kDiscovery,
        kLoggerComponent,
        "The sink service is initialized. Device discovery will start "
        "after user interaction.",
        "", "", "");
  } else {
    LoggerList::GetInstance()->Log(
        LoggerImpl::Severity::kInfo, mojom::LogCategory::kDiscovery,
        kLoggerComponent,
        "The sink service is initialized. Device discovery is starting soon.",
        "", "", "");
    StartMdnsDiscovery();
  }
}

void CastMediaSinkService::StopObservingPrefChanges() {
  local_state_change_registrar_.Reset();
}

std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
CastMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    MediaSinkServiceBase* dial_media_sink_service) {
  cast_channel::CastSocketService* cast_socket_service =
      cast_channel::CastSocketService::GetInstance();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      cast_socket_service->task_runner();

  local_state_change_registrar_.Init(g_browser_process->local_state());
  local_state_change_registrar_.Add(
      prefs::kMediaRouterCastAllowAllIPs,
      base::BindRepeating(&CastMediaSinkService::SetCastAllowAllIPs,
                          base::Unretained(this)));
  return std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
      new CastMediaSinkServiceImpl(
          sinks_discovered_cb, cast_socket_service,
          DiscoveryNetworkMonitor::GetInstance(), dial_media_sink_service,
          GetCastAllowAllIPsPref(g_browser_process->local_state())),
      base::OnTaskRunnerDeleter(task_runner));
}

void CastMediaSinkService::SetCastAllowAllIPs() {
  impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::SetCastAllowAllIPs,
                     base::Unretained(impl_.get()),
                     GetCastAllowAllIPsPref(g_browser_process->local_state())));
}

void CastMediaSinkService::StartMdnsDiscovery() {
  // `dns_sd_registry_ is already set to a mock version in unit tests only.
  // `impl_` must be initialized first because AddObserver might end up
  // calling `OnDnsSdEvent` right away.
  DCHECK(impl_);
  if (MdnsDiscoveryStarted()) {
    return;
  }

  dns_sd_registry_ = DnsSdRegistry::GetInstance();
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
  LoggerList::GetInstance()->Log(
      LoggerImpl::Severity::kInfo, mojom::LogCategory::kDiscovery,
      kLoggerComponent, "mDNS discovery started.", "", "", "");
}

bool CastMediaSinkService::MdnsDiscoveryStarted() const {
  return dns_sd_registry_ != nullptr;
}

void CastMediaSinkService::DiscoverSinksNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dns_sd_registry_) {
    dns_sd_registry_->ResetAndDiscover();
  } else {
    LoggerList::GetInstance()->Log(
        LoggerImpl::Severity::kInfo, mojom::LogCategory::kDiscovery,
        kLoggerComponent,
        "Failed to discover sinks. mDNS discovery hasn't started.", "", "", "");
  }

  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::OpenChannelsNow,
                                base::Unretained(impl_.get()), cast_sinks_));
}

void CastMediaSinkService::SetDnsSdRegistryForTest(DnsSdRegistry* registry) {
  DCHECK(!dns_sd_registry_);
  dns_sd_registry_ = registry;
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
}

void CastMediaSinkService::OnDnsSdEvent(
    const std::string& service_type,
    const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast_sinks_.clear();
  for (const auto& service : services) {
    // Create Cast sink from mDNS service description.
    MediaSinkInternal cast_sink;
    CreateCastMediaSinkResult result = CreateCastMediaSink(service, &cast_sink);
    if (result != CreateCastMediaSinkResult::kOk) {
      continue;
    }
    cast_sinks_.push_back(cast_sink);
  }

  impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannelsWithRandomizedDelay,
                     base::Unretained(impl_.get()), cast_sinks_,
                     CastMediaSinkServiceImpl::SinkSource::kMdns));
}

void CastMediaSinkService::OnDnsSdPermissionRejected() {
  // TODO(354232593): Pass the error to the MR.
  discovery_permission_rejected_cb_.Run();
}

void CastMediaSinkService::RunSinksDiscoveredCallback(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    std::vector<MediaSinkInternal> sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sinks_discovered_cb.Run(std::move(sinks));
}

}  // namespace media_router
