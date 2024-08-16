// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

#include "base/functional/bind.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

DualMediaSinkService* g_dual_media_sink_service = nullptr;

// static
DualMediaSinkService* DualMediaSinkService::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_dual_media_sink_service) {
    g_dual_media_sink_service = new DualMediaSinkService();
  }
  return g_dual_media_sink_service;
}

// static
bool DualMediaSinkService::HasInstance() {
  return g_dual_media_sink_service;
}

// static
DialMediaSinkServiceImpl* DualMediaSinkService::GetDialMediaSinkServiceImpl() {
  return dial_media_sink_service_->impl();
}

MediaSinkServiceBase* DualMediaSinkService::GetCastMediaSinkServiceBase() {
  return cast_media_sink_service_->impl();
}

CastMediaSinkServiceImpl* DualMediaSinkService::GetCastMediaSinkServiceImpl() {
  return cast_media_sink_service_->impl();
}

base::CallbackListSubscription DualMediaSinkService::AddSinksDiscoveredCallback(
    const OnSinksDiscoveredProviderCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& provider_and_sinks : current_sinks_) {
    callback.Run(provider_and_sinks.first, provider_and_sinks.second);
  }
  return sinks_discovered_callbacks_.Add(callback);
}

void DualMediaSinkService::SetDiscoveryPermissionRejectedCallback(
    base::RepeatingClosure discovery_permission_rejected_cb) {
  discovery_permission_rejected_cb_ = discovery_permission_rejected_cb;
}

void DualMediaSinkService::DiscoverSinksNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(imcheng): Move this call into CastMediaRouteProvider.
  if (cast_media_sink_service_) {
    cast_media_sink_service_->DiscoverSinksNow();
  }

  if (dial_media_sink_service_) {
    dial_media_sink_service_->DiscoverSinksNow();
  }
}

void DualMediaSinkService::StartDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartMdnsDiscovery();
  StartDialDiscovery();
}

void DualMediaSinkService::StartMdnsDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cast_media_sink_service_ &&
      !cast_media_sink_service_->MdnsDiscoveryStarted()) {
    cast_media_sink_service_->StartMdnsDiscovery();
  }
}

void DualMediaSinkService::StartDialDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dial_media_sink_service_ &&
      !dial_media_sink_service_->DiscoveryStarted()) {
    dial_media_sink_service_->StartDiscovery();
  }
}

bool DualMediaSinkService::MdnsDiscoveryStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cast_media_sink_service_
             ? cast_media_sink_service_->MdnsDiscoveryStarted()
             : false;
}

bool DualMediaSinkService::DialDiscoveryStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dial_media_sink_service_ &&
         dial_media_sink_service_->DiscoveryStarted();
}

DualMediaSinkService::DualMediaSinkService() {
  if (DialMediaRouteProviderEnabled()) {
    dial_media_sink_service_ = std::make_unique<DialMediaSinkService>();
    dial_media_sink_service_->Initialize(
        base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                            base::Unretained(this), "dial"));
  }

  cast_media_sink_service_ = std::make_unique<CastMediaSinkService>();
  cast_media_sink_service_->Initialize(
      base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                          base::Unretained(this), "cast"),
      base::BindRepeating(&DualMediaSinkService::OnDiscoveryPermissionRejected,
                          base::Unretained(this)),
      dial_media_sink_service_ ? dial_media_sink_service_->impl() : nullptr);

  cast_channel::CastSocketService* cast_socket_service =
      cast_channel::CastSocketService::GetInstance();
  cast_app_discovery_service_ = std::make_unique<CastAppDiscoveryServiceImpl>(
      GetCastMessageHandler(), cast_socket_service,
      cast_media_sink_service_->impl(), base::DefaultTickClock::GetInstance());
}

DualMediaSinkService::DualMediaSinkService(
    std::unique_ptr<CastMediaSinkService> cast_media_sink_service,
    std::unique_ptr<DialMediaSinkService> dial_media_sink_service,
    std::unique_ptr<CastAppDiscoveryService> cast_app_discovery_service)
    : dial_media_sink_service_(std::move(dial_media_sink_service)),
      cast_media_sink_service_(std::move(cast_media_sink_service)),
      cast_app_discovery_service_(std::move(cast_app_discovery_service)) {}

DualMediaSinkService::~DualMediaSinkService() = default;

void DualMediaSinkService::OnSinksDiscovered(
    const std::string& provider_name,
    std::vector<MediaSinkInternal> sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& sinks_for_provider = current_sinks_[provider_name];
  sinks_for_provider = std::move(sinks);
  sinks_discovered_callbacks_.Notify(provider_name, sinks_for_provider);
}

void DualMediaSinkService::OnDiscoveryPermissionRejected() {
  discovery_permission_rejected_cb_.Run();
}

void DualMediaSinkService::AddLogger(LoggerImpl* logger_impl) {
  LoggerList::GetInstance()->AddLogger(logger_impl);
}

void DualMediaSinkService::RemoveLogger(LoggerImpl* logger_impl) {
  LoggerList::GetInstance()->RemoveLogger(logger_impl);
}

void DualMediaSinkService::StopObservingPrefChanges() {
  cast_media_sink_service_->StopObservingPrefChanges();
}

}  // namespace media_router
