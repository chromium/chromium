// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/extension/extension_media_route_provider_proxy.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"

namespace media_router {

ExtensionMediaRouteProviderProxy::ExtensionMediaRouteProviderProxy(
    content::BrowserContext* context)
    : request_manager_(
          EventPageRequestManagerFactory::GetApiForBrowserContext(context)) {}

ExtensionMediaRouteProviderProxy::~ExtensionMediaRouteProviderProxy() = default;

void ExtensionMediaRouteProviderProxy::Bind(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver) {
  // This method is called when the previous receiver became invalid. We close
  // it first to make sure it is in a clean state.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ExtensionMediaRouteProviderProxy::CreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& original_presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    CreateRouteCallback callback) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoCreateRoute,
                     weak_factory_.GetWeakPtr(), media_source, sink_id,
                     original_presentation_id, origin, tab_id, timeout,
                     incognito, std::move(callback)),
      MediaRouteProviderWakeReason::CREATE_ROUTE);
}

void ExtensionMediaRouteProviderProxy::JoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    JoinRouteCallback callback) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoJoinRoute,
                     weak_factory_.GetWeakPtr(), media_source, presentation_id,
                     origin, tab_id, timeout, incognito, std::move(callback)),
      MediaRouteProviderWakeReason::JOIN_ROUTE);
}

void ExtensionMediaRouteProviderProxy::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoConnectRouteByRouteId,
                     weak_factory_.GetWeakPtr(), media_source, route_id,
                     presentation_id, origin, tab_id, timeout, incognito,
                     std::move(callback)),
      MediaRouteProviderWakeReason::CONNECT_ROUTE_BY_ROUTE_ID);
}

void ExtensionMediaRouteProviderProxy::TerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  DVLOG(2) << "TerminateRoute " << route_id;
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoTerminateRoute,
                     weak_factory_.GetWeakPtr(), route_id, std::move(callback)),
      MediaRouteProviderWakeReason::TERMINATE_ROUTE);
}

void ExtensionMediaRouteProviderProxy::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoSendRouteMessage,
                     weak_factory_.GetWeakPtr(), media_route_id, message),
      MediaRouteProviderWakeReason::SEND_SESSION_MESSAGE);
}

void ExtensionMediaRouteProviderProxy::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoSendRouteBinaryMessage,
          weak_factory_.GetWeakPtr(), media_route_id, data),
      MediaRouteProviderWakeReason::SEND_SESSION_BINARY_MESSAGE);
}

void ExtensionMediaRouteProviderProxy::StartObservingMediaSinks(
    const std::string& media_source) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoStartObservingMediaSinks,
          weak_factory_.GetWeakPtr(), media_source),
      MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_SINKS);
}

void ExtensionMediaRouteProviderProxy::StopObservingMediaSinks(
    const std::string& media_source) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoStopObservingMediaSinks,
          weak_factory_.GetWeakPtr(), media_source),
      MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_ROUTES);
}

void ExtensionMediaRouteProviderProxy::StartObservingMediaRoutes(
    const std::string& media_source) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoStartObservingMediaRoutes,
          weak_factory_.GetWeakPtr(), media_source),
      MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_ROUTES);
}

void ExtensionMediaRouteProviderProxy::StopObservingMediaRoutes(
    const std::string& media_source) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoStopObservingMediaRoutes,
          weak_factory_.GetWeakPtr(), media_source),
      MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_ROUTES);
}

void ExtensionMediaRouteProviderProxy::StartListeningForRouteMessages(
    const std::string& route_id) {
  request_manager_->RunOrDefer(
      base::Bind(
          &ExtensionMediaRouteProviderProxy::DoStartListeningForRouteMessages,
          weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::START_LISTENING_FOR_ROUTE_MESSAGES);
}

void ExtensionMediaRouteProviderProxy::StopListeningForRouteMessages(
    const std::string& route_id) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoStopListeningForRouteMessages,
          weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::STOP_LISTENING_FOR_ROUTE_MESSAGES);
}

void ExtensionMediaRouteProviderProxy::DetachRoute(
    const std::string& route_id) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoDetachRoute,
                     weak_factory_.GetWeakPtr(), route_id),
      MediaRouteProviderWakeReason::DETACH_ROUTE);
}

void ExtensionMediaRouteProviderProxy::EnableMdnsDiscovery() {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoEnableMdnsDiscovery,
                     weak_factory_.GetWeakPtr()),
      MediaRouteProviderWakeReason::ENABLE_MDNS_DISCOVERY);
}

void ExtensionMediaRouteProviderProxy::UpdateMediaSinks(
    const std::string& media_source) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoUpdateMediaSinks,
                     weak_factory_.GetWeakPtr(), media_source),
      MediaRouteProviderWakeReason::UPDATE_MEDIA_SINKS);
}

void ExtensionMediaRouteProviderProxy::SearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoSearchSinks,
                     weak_factory_.GetWeakPtr(), sink_id, media_source,
                     std::move(search_criteria), std::move(callback)),
      MediaRouteProviderWakeReason::SEARCH_SINKS);
}

void ExtensionMediaRouteProviderProxy::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  DVLOG(1) << "ProvideSinks called with " << sinks.size()
           << " sinks from provider: " << provider_name;
  request_manager_->RunOrDefer(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::DoProvideSinks,
                     weak_factory_.GetWeakPtr(), provider_name, sinks),
      MediaRouteProviderWakeReason::PROVIDE_SINKS);
}

void ExtensionMediaRouteProviderProxy::CreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  request_manager_->RunOrDefer(
      base::BindOnce(
          &ExtensionMediaRouteProviderProxy::DoCreateMediaRouteController,
          weak_factory_.GetWeakPtr(), route_id, std::move(media_controller),
          std::move(observer), std::move(callback)),
      MediaRouteProviderWakeReason::CREATE_MEDIA_ROUTE_CONTROLLER);
}

void ExtensionMediaRouteProviderProxy::RegisterMediaRouteProvider(
    mojo::PendingRemote<mojom::MediaRouteProvider> media_route_provider) {
  media_route_provider_.reset();
  media_route_provider_.Bind(std::move(media_route_provider));
  media_route_provider_.set_disconnect_handler(
      base::BindOnce(&ExtensionMediaRouteProviderProxy::OnMojoConnectionError,
                     base::Unretained(this)));
  request_manager_->OnMojoConnectionsReady();
}

void ExtensionMediaRouteProviderProxy::OnMojoConnectionError() {
  request_manager_->OnMojoConnectionError();
  media_route_provider_.reset();
}

void ExtensionMediaRouteProviderProxy::SetExtensionId(
    const std::string& extension_id) {
  request_manager_->SetExtensionId(extension_id);
}

void ExtensionMediaRouteProviderProxy::DoCreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& original_presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    CreateRouteCallback callback) {
  DVLOG(1) << "DoCreateRoute " << media_source << "=>" << sink_id
           << ", presentation ID: " << original_presentation_id;
  media_route_provider_->CreateRoute(media_source, sink_id,
                                     original_presentation_id, origin, tab_id,
                                     timeout, incognito, std::move(callback));
}

void ExtensionMediaRouteProviderProxy::DoJoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    JoinRouteCallback callback) {
  DVLOG(1) << "DoJoinRoute " << media_source
           << ", presentation ID: " << presentation_id;
  media_route_provider_->JoinRoute(media_source, presentation_id, origin,
                                   tab_id, timeout, incognito,
                                   std::move(callback));
}

void ExtensionMediaRouteProviderProxy::DoConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  DVLOG(1) << "DoConnectRouteByRouteId " << media_source
           << ", route ID: " << route_id
           << ", presentation ID: " << presentation_id;
  media_route_provider_->ConnectRouteByRouteId(
      media_source, route_id, presentation_id, origin, tab_id, timeout,
      incognito, std::move(callback));
}

void ExtensionMediaRouteProviderProxy::DoTerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  DVLOG(1) << "DoTerminateRoute " << route_id;
  media_route_provider_->TerminateRoute(route_id, std::move(callback));
}

void ExtensionMediaRouteProviderProxy::DoSendRouteMessage(
    const std::string& media_route_id,
    const std::string& message) {
  DVLOG(1) << "DoSendRouteMessage " << media_route_id;
  media_route_provider_->SendRouteMessage(media_route_id, message);
}

void ExtensionMediaRouteProviderProxy::DoSendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  DVLOG(1) << "DoSendRouteBinaryMessage " << media_route_id;
  media_route_provider_->SendRouteBinaryMessage(media_route_id, data);
}

void ExtensionMediaRouteProviderProxy::DoStartObservingMediaSinks(
    const std::string& media_source) {
  DVLOG(1) << "DoStartObservingMediaSinks: " << media_source;
  media_route_provider_->StartObservingMediaSinks(media_source);
}

void ExtensionMediaRouteProviderProxy::DoStopObservingMediaSinks(
    const std::string& media_source) {
  DVLOG(1) << "DoStopObservingMediaSinks: " << media_source;
  media_route_provider_->StopObservingMediaSinks(media_source);
}

void ExtensionMediaRouteProviderProxy::DoStartObservingMediaRoutes(
    const std::string& media_source) {
  DVLOG(1) << "DoStartObservingMediaRoutes: " << media_source;
  media_route_provider_->StartObservingMediaRoutes(media_source);
}

void ExtensionMediaRouteProviderProxy::DoStopObservingMediaRoutes(
    const std::string& media_source) {
  DVLOG(1) << "DoStopObservingMediaRoutes: " << media_source;
  media_route_provider_->StopObservingMediaRoutes(media_source);
}

void ExtensionMediaRouteProviderProxy::DoStartListeningForRouteMessages(
    const std::string& route_id) {
  DVLOG(1) << "DoStartListeningForRouteMessages";
  media_route_provider_->StartListeningForRouteMessages(route_id);
}

void ExtensionMediaRouteProviderProxy::DoStopListeningForRouteMessages(
    const std::string& route_id) {
  DVLOG(1) << "StopListeningForRouteMessages";
  media_route_provider_->StopListeningForRouteMessages(route_id);
}

void ExtensionMediaRouteProviderProxy::DoDetachRoute(
    const std::string& route_id) {
  DVLOG(1) << "DoDetachRoute " << route_id;
  media_route_provider_->DetachRoute(route_id);
}

void ExtensionMediaRouteProviderProxy::DoEnableMdnsDiscovery() {
  DVLOG(1) << "DoEnsureMdnsDiscoveryEnabled";
  media_route_provider_->EnableMdnsDiscovery();
}

void ExtensionMediaRouteProviderProxy::DoUpdateMediaSinks(
    const std::string& media_source) {
  DVLOG(1) << "DoUpdateMediaSinks: " << media_source;
  media_route_provider_->UpdateMediaSinks(media_source);
}

void ExtensionMediaRouteProviderProxy::DoSearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  DVLOG(1) << "SearchSinks";
  media_route_provider_->SearchSinks(
      sink_id, media_source, std::move(search_criteria), std::move(callback));
}

void ExtensionMediaRouteProviderProxy::DoProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  DVLOG(1) << "DoProvideSinks";
  media_route_provider_->ProvideSinks(provider_name, sinks);
}

void ExtensionMediaRouteProviderProxy::DoCreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  DVLOG(1) << "DoCreateMediaRouteController";
  if (!media_controller.is_valid() || !observer.is_valid())
    return;

  media_route_provider_->CreateMediaRouteController(
      route_id, std::move(media_controller), std::move(observer),
      std::move(callback));
}

}  // namespace media_router
