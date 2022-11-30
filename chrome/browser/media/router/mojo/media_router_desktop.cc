// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/openscreen_platform/network_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#endif

namespace media_router {

#if BUILDFLAG(IS_WIN)
constexpr char kLoggerComponent[] = "MediaRouterDesktop";
#endif

MediaRouterDesktop::~MediaRouterDesktop() {
  if (media_sink_service_)
    media_sink_service_->RemoveLogger(GetLogger());
}

void MediaRouterDesktop::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::OnUserGesture();
  if (!media_sink_service_)
    return;

  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSource::ForUnchosenDesktop().id());

  media_sink_service_->OnUserGesture();
  if (!media_sink_service_subscription_) {
    media_sink_service_subscription_ =
        media_sink_service_->AddSinksDiscoveredCallback(
            base::BindRepeating(&MediaSinkServiceStatus::UpdateDiscoveredSinks,
                                media_sink_service_status_.GetWeakPtr()));
  }

#if BUILDFLAG(IS_WIN)
  if (!media_sink_service_->MdnsDiscoveryStarted()) {
    GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The user interacted with MR. mDNS discovery is enabled.", "", "", "");
  }
  EnsureMdnsDiscoveryEnabled();
#endif
}

base::Value::Dict MediaRouterDesktop::GetState() const {
  return media_sink_service_status_.GetStatusAsValue();
}

void MediaRouterDesktop::GetProviderState(
    mojom::MediaRouteProviderId provider_id,
    mojom::MediaRouteProvider::GetStateCallback callback) const {
  if (provider_id == mojom::MediaRouteProviderId::CAST) {
    media_route_providers_.at(provider_id)->GetState(std::move(callback));
  } else {
    std::move(callback).Run(mojom::ProviderStatePtr());
  }
}

absl::optional<mojom::MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForPresentation(
    const std::string& presentation_id) {
  // TODO(takumif): Once the Android Media Router also uses MediaRouterMojoImpl,
  // we must support these presentation IDs in Android as well.
  if (presentation_id == kAutoJoinPresentationId ||
      base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    return mojom::MediaRouteProviderId::CAST;
  }
  return MediaRouterMojoImpl::GetProviderIdForPresentation(presentation_id);
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context)
    : MediaRouterMojoImpl(context),
      cast_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      dial_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      media_sink_service_(base::CommandLine::ForCurrentProcess()->HasSwitch(
                              kDisableMediaRouteProvidersForTestSwitch)
                              ? nullptr
                              : DualMediaSinkService::GetInstance()) {}

void MediaRouterDesktop::RegisterMediaRouteProvider(
    mojom::MediaRouteProviderId provider_id,
    mojo::PendingRemote<mojom::MediaRouteProvider>
        media_route_provider_remote) {
  mojo::Remote<mojom::MediaRouteProvider> bound_remote(
      std::move(media_route_provider_remote));
  bound_remote.set_disconnect_handler(
      base::BindOnce(&MediaRouterDesktop::OnProviderConnectionError,
                     weak_factory_.GetWeakPtr(), provider_id));
  media_route_providers_[provider_id] = std::move(bound_remote);

  SyncStateToMediaRouteProvider(provider_id);
}

void MediaRouterDesktop::OnSinksReceived(
    mojom::MediaRouteProviderId provider_id,
    const std::string& media_source,
    const std::vector<MediaSinkInternal>& internal_sinks,
    const std::vector<url::Origin>& origins) {
  media_sink_service_status_.UpdateAvailableSinks(provider_id, media_source,
                                                  internal_sinks);
  MediaRouterMojoImpl::OnSinksReceived(provider_id, media_source,
                                       internal_sinks, origins);
}

void MediaRouterDesktop::GetMediaSinkServiceStatus(
    mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) {
  std::move(callback).Run(media_sink_service_status_.GetStatusAsJSONString());
}

void MediaRouterDesktop::Initialize() {
  MediaRouterMojoImpl::Initialize();
  if (media_sink_service_) {
    media_sink_service_->AddLogger(GetLogger());
    InitializeMediaRouteProviders();
#if BUILDFLAG(IS_WIN)
    CanFirewallUseLocalPorts(
        base::BindOnce(&MediaRouterDesktop::OnFirewallCheckComplete,
                       weak_factory_.GetWeakPtr()));
#endif
  }
}

void MediaRouterDesktop::InitializeMediaRouteProviders() {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableMediaRouteProvidersForTestSwitch));

  if (!openscreen_platform::HasNetworkContextGetter()) {
    openscreen_platform::SetNetworkContextGetter(base::BindRepeating([] {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      return g_browser_process->system_network_context_manager()->GetContext();
    }));
  }

  InitializeWiredDisplayMediaRouteProvider();
  InitializeCastMediaRouteProvider();
  if (DialMediaRouteProviderEnabled()) {
    InitializeDialMediaRouteProvider();
  }
}

void MediaRouterDesktop::InitializeWiredDisplayMediaRouteProvider() {
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  MediaRouterMojoImpl::BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::MediaRouteProvider> wired_display_provider_remote;
  wired_display_provider_ = std::make_unique<WiredDisplayMediaRouteProvider>(
      wired_display_provider_remote.InitWithNewPipeAndPassReceiver(),
      std::move(media_router_remote), Profile::FromBrowserContext(context()));
  RegisterMediaRouteProvider(mojom::MediaRouteProviderId::WIRED_DISPLAY,
                             std::move(wired_display_provider_remote));
}

std::string MediaRouterDesktop::GetHashToken() {
  return GetReceiverIdHashToken(
      Profile::FromBrowserContext(context())->GetPrefs());
}

void MediaRouterDesktop::InitializeCastMediaRouteProvider() {
  DCHECK(media_sink_service_);
  auto task_runner =
      cast_channel::CastSocketService::GetInstance()->task_runner();
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  MediaRouterMojoImpl::BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::MediaRouteProvider> cast_provider_remote;
  cast_provider_ =
      std::unique_ptr<CastMediaRouteProvider, base::OnTaskRunnerDeleter>(
          new CastMediaRouteProvider(
              cast_provider_remote.InitWithNewPipeAndPassReceiver(),
              std::move(media_router_remote),
              media_sink_service_->GetCastMediaSinkServiceBase(),
              media_sink_service_->cast_app_discovery_service(),
              GetCastMessageHandler(), GetHashToken(), task_runner),
          base::OnTaskRunnerDeleter(task_runner));
  RegisterMediaRouteProvider(mojom::MediaRouteProviderId::CAST,
                             std::move(cast_provider_remote));
}

void MediaRouterDesktop::InitializeDialMediaRouteProvider() {
  DCHECK(media_sink_service_);
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  MediaRouterMojoImpl::BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::MediaRouteProvider> dial_provider_remote;

  auto* dial_media_sink_service =
      media_sink_service_->GetDialMediaSinkServiceImpl();
  auto task_runner = dial_media_sink_service->task_runner();

  dial_provider_ =
      std::unique_ptr<DialMediaRouteProvider, base::OnTaskRunnerDeleter>(
          new DialMediaRouteProvider(
              dial_provider_remote.InitWithNewPipeAndPassReceiver(),
              std::move(media_router_remote), dial_media_sink_service,
              GetHashToken(), task_runner),
          base::OnTaskRunnerDeleter(task_runner));
  RegisterMediaRouteProvider(mojom::MediaRouteProviderId::DIAL,
                             std::move(dial_provider_remote));
}

#if BUILDFLAG(IS_WIN)
void MediaRouterDesktop::EnsureMdnsDiscoveryEnabled() {
  DCHECK(media_sink_service_);
  media_sink_service_->StartMdnsDiscovery();
}

void MediaRouterDesktop::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports) {
    GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "Windows firewall allows mDNS. Ensuring mDNS discovery is enabled.", "",
        "", "");
    EnsureMdnsDiscoveryEnabled();
  } else {
    GetLogger()->LogInfo(mojom::LogCategory::kDiscovery, kLoggerComponent,
                         "Windows firewall does not allows mDNS. mDNS "
                         "discovery can be enabled by user gesture.",
                         "", "", "");
  }
}
#endif

}  // namespace media_router
