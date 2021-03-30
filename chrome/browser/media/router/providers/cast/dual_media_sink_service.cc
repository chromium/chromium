// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

#include "base/bind.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "components/cast_channel/cast_socket_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

DualMediaSinkService* DualMediaSinkService::instance_for_test_ = nullptr;

// static
DualMediaSinkService* DualMediaSinkService::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (instance_for_test_)
    return instance_for_test_;

  static DualMediaSinkService* instance = new DualMediaSinkService();
  return instance;
}

// static
void DualMediaSinkService::SetInstanceForTest(
    DualMediaSinkService* instance_for_test) {
  instance_for_test_ = instance_for_test;
}

DialMediaSinkServiceImpl* DualMediaSinkService::GetDialMediaSinkServiceImpl() {
  return dial_media_sink_service_->impl();
}

MediaSinkServiceBase* DualMediaSinkService::GetCastMediaSinkServiceImpl() {
  return cast_media_sink_service_->impl();
}

base::CallbackListSubscription DualMediaSinkService::AddSinksDiscoveredCallback(
    const OnSinksDiscoveredProviderCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sinks_discovered_callbacks_.Add(callback);
}

void DualMediaSinkService::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(imcheng): Move this call into CastMediaRouteProvider.
  if (cast_media_sink_service_)
    cast_media_sink_service_->OnUserGesture();
}

void DualMediaSinkService::StartMdnsDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cast_media_sink_service_)
    cast_media_sink_service_->StartMdnsDiscovery();
}

bool DualMediaSinkService::MdnsDiscoveryStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cast_media_sink_service_
             ? cast_media_sink_service_->MdnsDiscoveryStarted()
             : false;
}

DualMediaSinkService::DualMediaSinkService() {
  dial_media_sink_service_ = std::make_unique<DialMediaSinkService>();
  dial_media_sink_service_->Start(
      base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                          base::Unretained(this), "dial"));

  cast_media_sink_service_ = std::make_unique<CastMediaSinkService>();
  cast_media_sink_service_->Start(
      base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                          base::Unretained(this), "cast"),
      dial_media_sink_service_->impl());

  if (CastMediaRouteProviderEnabled()) {
    cast_channel::CastSocketService* cast_socket_service =
        cast_channel::CastSocketService::GetInstance();
    cast_app_discovery_service_ = std::make_unique<CastAppDiscoveryServiceImpl>(
        GetCastMessageHandler(), cast_socket_service,
        cast_media_sink_service_->impl(),
        base::DefaultTickClock::GetInstance());
  }
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

void DualMediaSinkService::BindLogger(LoggerImpl* logger_impl) {
  if (logger_is_bound_)
    return;
  logger_is_bound_ = true;
  cast_media_sink_service_->BindLogger(logger_impl);

  mojo::PendingRemote<mojom::Logger> dial_pending_remote;
  logger_impl->Bind(dial_pending_remote.InitWithNewPipeAndPassReceiver());
  dial_media_sink_service_->BindLogger(std::move(dial_pending_remote));
  if (!CastMediaRouteProviderEnabled())
    return;
  mojo::PendingRemote<mojom::Logger> cast_discovery_pending_remote;
  logger_impl->Bind(
      cast_discovery_pending_remote.InitWithNewPipeAndPassReceiver());
  cast_app_discovery_service_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastAppDiscoveryService::BindLogger,
                     base::Unretained(cast_app_discovery_service_.get()),
                     std::move(cast_discovery_pending_remote)));
}

}  // namespace media_router
