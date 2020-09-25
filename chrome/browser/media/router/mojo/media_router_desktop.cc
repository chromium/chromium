// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_source.h"
#include "components/openscreen_platform/network_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#if defined(OS_WIN)
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#endif

namespace media_router {

MediaRouterDesktop::~MediaRouterDesktop() = default;

// static
void MediaRouterDesktop::BindToReceiver(
    const extensions::Extension* extension,
    content::BrowserContext* context,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::MediaRouter> receiver) {
  MediaRouterDesktop* impl = static_cast<MediaRouterDesktop*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoReceiver(std::move(receiver), *extension);
}

void MediaRouterDesktop::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::OnUserGesture();
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSource::ForUnchosenDesktop().id());

  media_sink_service_->BindLogger(GetLogger());
  media_sink_service_->OnUserGesture();

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

base::Value MediaRouterDesktop::GetState() const {
  return media_sink_service_status_.GetStatusAsValue();
}

void MediaRouterDesktop::GetProviderState(
    MediaRouteProviderId provider_id,
    mojom::MediaRouteProvider::GetStateCallback callback) const {
  if (provider_id == MediaRouteProviderId::CAST &&
      CastMediaRouteProviderEnabled()) {
    media_route_providers_.at(provider_id)->GetState(std::move(callback));
  } else {
    std::move(callback).Run(mojom::ProviderStatePtr());
  }
}

base::Optional<MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForPresentation(
    const std::string& presentation_id) {
  // TODO(takumif): Once the Android Media Router also uses MediaRouterMojoImpl,
  // we must support these presentation IDs in Android as well.
  if (presentation_id == kAutoJoinPresentationId ||
      base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    return CastMediaRouteProviderEnabled() ? MediaRouteProviderId::CAST
                                           : MediaRouteProviderId::EXTENSION;
  }
  return MediaRouterMojoImpl::GetProviderIdForPresentation(presentation_id);
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context)
    : MediaRouterDesktop(context, DualMediaSinkService::GetInstance()) {
#if defined(OS_WIN)
  CanFirewallUseLocalPorts(
      base::BindOnce(&MediaRouterDesktop::OnFirewallCheckComplete,
                     weak_factory_.GetWeakPtr()));
#endif
}

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context,
                                       DualMediaSinkService* media_sink_service)
    : MediaRouterMojoImpl(context),
      cast_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      dial_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      media_sink_service_(media_sink_service) {
  InitializeMediaRouteProviders();
}

void MediaRouterDesktop::RegisterMediaRouteProvider(
    MediaRouteProviderId provider_id,
    mojo::PendingRemote<mojom::MediaRouteProvider> media_route_provider_remote,
    mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) {
  auto config = mojom::MediaRouteProviderConfig::New();
  // Enabling browser side discovery / sink query means disabling extension side
  // discovery / sink query. We are migrating discovery from the external Media
  // Route Provider to the Media Router (https://crbug.com/687383), so we need
  // to disable it in the provider.
  //
  // FIXME: Remove config flags once all features are launched
  config->enable_cast_discovery = false;
  config->enable_dial_sink_query = false;
  config->enable_cast_sink_query = !CastMediaRouteProviderEnabled();
  config->use_mirroring_service = true;
  std::move(callback).Run(instance_id(), std::move(config));

  SyncStateToMediaRouteProvider(provider_id);

  if (provider_id == MediaRouteProviderId::EXTENSION) {
    RegisterExtensionMediaRouteProvider(std::move(media_route_provider_remote));
  } else {
    mojo::Remote<mojom::MediaRouteProvider> bound_remote(
        std::move(media_route_provider_remote));
    bound_remote.set_disconnect_handler(
        base::BindOnce(&MediaRouterDesktop::OnProviderConnectionError,
                       weak_factory_.GetWeakPtr(), provider_id));
    media_route_providers_[provider_id] = std::move(bound_remote);
  }
}

void MediaRouterDesktop::OnSinksReceived(
    MediaRouteProviderId provider_id,
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

void MediaRouterDesktop::RegisterExtensionMediaRouteProvider(
    mojo::PendingRemote<mojom::MediaRouteProvider> extension_provider_remote) {
  ProvideSinksToExtension();
#if defined(OS_WIN)
  // The extension MRP already turns on mDNS discovery for platforms other than
  // Windows. It only relies on this signalling from MR on Windows to avoid
  // triggering a firewall prompt out of the context of MR from the user's
  // perspective. This particular call reminds the extension to enable mDNS
  // discovery when it wakes up, has been upgraded, etc.
  if (should_enable_mdns_discovery_)
    EnsureMdnsDiscoveryEnabled();
#endif
  extension_provider_proxy_->RegisterMediaRouteProvider(
      std::move(extension_provider_remote));
}

void MediaRouterDesktop::BindToMojoReceiver(
    mojo::PendingReceiver<mojom::MediaRouter> receiver,
    const extensions::Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaRouterMojoImpl::BindToMojoReceiver(std::move(receiver));
  extension_provider_proxy_->SetExtensionId(extension.id());
  if (!provider_version_was_recorded_) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(extension);
    provider_version_was_recorded_ = true;
  }
}

void MediaRouterDesktop::ProvideSinksToExtension() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If calling |ProvideSinksToExtension| for the first time, add a callback to
  // be notified of sink updates.
  if (!media_sink_service_subscription_) {
    media_sink_service_subscription_ =
        media_sink_service_->AddSinksDiscoveredCallback(base::BindRepeating(
            &MediaRouterDesktop::ProvideSinks, base::Unretained(this)));
  }

  // Sync the current list of sinks to the extension.
  for (const auto& provider_and_sinks : media_sink_service_->current_sinks())
    ProvideSinks(provider_and_sinks.first, provider_and_sinks.second);
}

void MediaRouterDesktop::ProvideSinks(
    const std::string& provider_name,
    const std::vector<MediaSinkInternal>& sinks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We no longer provide DIAL sources to the extension.
  constexpr char kDialSourceName[] = "dial";
  if (provider_name == kDialSourceName) {
    return;
  }
  media_route_providers_[MediaRouteProviderId::EXTENSION]->ProvideSinks(
      provider_name, sinks);

  media_sink_service_status_.UpdateDiscoveredSinks(provider_name, sinks);
}

void MediaRouterDesktop::InitializeMediaRouteProviders() {
  if (!openscreen_platform::HasNetworkContextGetter()) {
    openscreen_platform::SetNetworkContextGetter(base::BindRepeating([] {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      return g_browser_process->system_network_context_manager()->GetContext();
    }));
  }

  InitializeExtensionMediaRouteProviderProxy();
  InitializeWiredDisplayMediaRouteProvider();
  if (CastMediaRouteProviderEnabled())
    InitializeCastMediaRouteProvider();
  if (DialMediaRouteProviderEnabled())
    InitializeDialMediaRouteProvider();
}

void MediaRouterDesktop::InitializeExtensionMediaRouteProviderProxy() {
  if (!extension_provider_proxy_) {
    extension_provider_proxy_ =
        std::make_unique<ExtensionMediaRouteProviderProxy>(context());
  }
  mojo::Remote<mojom::MediaRouteProvider> extension_provider_proxy_remote;
  extension_provider_proxy_->Bind(
      extension_provider_proxy_remote.BindNewPipeAndPassReceiver());
  extension_provider_proxy_remote.set_disconnect_handler(base::BindOnce(
      &MediaRouterDesktop::OnExtensionProviderError, base::Unretained(this)));
  media_route_providers_[MediaRouteProviderId::EXTENSION] =
      std::move(extension_provider_proxy_remote);
}

void MediaRouterDesktop::OnExtensionProviderError() {
  // The message pipe for |extension_provider_proxy_| might error out due to
  // Media Router extension causing dropped callbacks. Detect this case and
  // recover by re-creating the pipe.
  if (extension_provider_error_count_ >= kMaxMediaRouteProviderErrorCount)
    return;

  ++extension_provider_error_count_;
  InitializeExtensionMediaRouteProviderProxy();
}

void MediaRouterDesktop::InitializeWiredDisplayMediaRouteProvider() {
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  MediaRouterMojoImpl::BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::MediaRouteProvider> wired_display_provider_remote;
  wired_display_provider_ = std::make_unique<WiredDisplayMediaRouteProvider>(
      wired_display_provider_remote.InitWithNewPipeAndPassReceiver(),
      std::move(media_router_remote), Profile::FromBrowserContext(context()));
  RegisterMediaRouteProvider(MediaRouteProviderId::WIRED_DISPLAY,
                             std::move(wired_display_provider_remote),
                             base::DoNothing());
}

std::string MediaRouterDesktop::GetHashToken() {
  return GetReceiverIdHashToken(
      Profile::FromBrowserContext(context())->GetPrefs());
}

void MediaRouterDesktop::InitializeCastMediaRouteProvider() {
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
              media_sink_service_->GetCastMediaSinkServiceImpl(),
              media_sink_service_->cast_app_discovery_service(),
              GetCastMessageHandler(), GetHashToken(), task_runner),
          base::OnTaskRunnerDeleter(task_runner));
  RegisterMediaRouteProvider(MediaRouteProviderId::CAST,
                             std::move(cast_provider_remote),
                             base::DoNothing());
}

void MediaRouterDesktop::InitializeDialMediaRouteProvider() {
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
  RegisterMediaRouteProvider(MediaRouteProviderId::DIAL,
                             std::move(dial_provider_remote),
                             base::DoNothing());
}

#if defined(OS_WIN)
void MediaRouterDesktop::EnsureMdnsDiscoveryEnabled() {
  media_sink_service_->StartMdnsDiscovery();
  // Record that we enabled mDNS discovery, so that we will know to enable again
  // when we reconnect to the component extension.
  should_enable_mdns_discovery_ = true;
}

void MediaRouterDesktop::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

}  // namespace media_router
