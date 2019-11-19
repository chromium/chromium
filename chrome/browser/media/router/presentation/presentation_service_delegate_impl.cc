// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/small_map.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/media/router/presentation/browser_presentation_connection_proxy.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager_factory.h"
#include "chrome/browser/media/router/presentation/presentation_media_sinks_observer.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

using blink::mojom::PresentationConnection;
using blink::mojom::PresentationError;
using blink::mojom::PresentationErrorType;
using blink::mojom::PresentationInfo;
using blink::mojom::ScreenAvailability;
using content::RenderFrameHost;

namespace media_router {

namespace {

using DelegateObserver = content::PresentationServiceDelegate::Observer;

// Gets the last committed URL for the render frame specified by
// |render_frame_host_id|.
url::Origin GetLastCommittedURLForFrame(
    content::GlobalFrameRoutingId render_frame_host_id) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      render_frame_host_id.child_id, render_frame_host_id.frame_routing_id);
  DCHECK(render_frame_host);
  return render_frame_host->GetLastCommittedOrigin();
}

bool ArePresentationRequestsEqual(
    const content::PresentationRequest& request1,
    const content::PresentationRequest& request2) {
  return request1.render_frame_host_id == request2.render_frame_host_id &&
         request1.presentation_urls == request2.presentation_urls &&
         ((request1.frame_origin.opaque() && request2.frame_origin.opaque()) ||
          (request1.frame_origin == request2.frame_origin));
}

}  // namespace

// PresentationFrame interfaces with MediaRouter to maintain the current state
// of Presentation API within a single render frame, such as the set of
// PresentationAvailability listeners and PresentationConnections.
// Instances are lazily created when certain Presentation API is invoked on a
// frame, and are owned by PresentationServiceDelegateImpl.
// Instances are destroyed when the corresponding frame navigates, or when it
// is destroyed.
class PresentationFrame {
 public:
  PresentationFrame(const content::GlobalFrameRoutingId& render_frame_host_id,
                    content::WebContents* web_contents,
                    MediaRouter* router);
  ~PresentationFrame();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  void RemoveScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool HasScreenAvailabilityListenerForTest(
      const MediaSource::Id& source_id) const;
  void ListenForConnectionStateChange(
      const PresentationInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);

  void Reset();

  MediaRoute::Id GetRouteId(const std::string& presentation_id) const;

  void AddPresentation(const PresentationInfo& presentation_info,
                       const MediaRoute& route);
  void ConnectToPresentation(
      const PresentationInfo& presentation_info,
      mojo::PendingRemote<PresentationConnection> controller_connection_remote,
      mojo::PendingReceiver<PresentationConnection>
          receiver_connection_receiver);
  void RemovePresentation(const std::string& presentation_id);

 private:
  base::small_map<std::map<std::string, MediaRoute>> presentation_id_to_route_;
  base::small_map<
      std::map<std::string, std::unique_ptr<PresentationMediaSinksObserver>>>
      url_to_sinks_observer_;
  std::unordered_map<MediaRoute::Id,
                     std::unique_ptr<PresentationConnectionStateSubscription>>
      connection_state_subscriptions_;
  std::unordered_map<MediaRoute::Id,
                     std::unique_ptr<BrowserPresentationConnectionProxy>>
      browser_connection_proxies_;

  content::GlobalFrameRoutingId render_frame_host_id_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  content::WebContents* web_contents_;
  MediaRouter* router_;
};

PresentationFrame::PresentationFrame(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    content::WebContents* web_contents,
    MediaRouter* router)
    : render_frame_host_id_(render_frame_host_id),
      web_contents_(web_contents),
      router_(router) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrame::~PresentationFrame() = default;

MediaRoute::Id PresentationFrame::GetRouteId(
    const std::string& presentation_id) const {
  auto it = presentation_id_to_route_.find(presentation_id);
  return it != presentation_id_to_route_.end() ? it->second.media_route_id()
                                               : "";
}

bool PresentationFrame::SetScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  GURL url = listener->GetAvailabilityUrl();
  if (!IsValidPresentationUrl(url)) {
    listener->OnScreenAvailabilityChanged(
        ScreenAvailability::SOURCE_NOT_SUPPORTED);
    return false;
  }

  MediaRouterMetrics::RecordPresentationUrlType(url);

  MediaSource source = MediaSource::ForPresentationUrl(url);
  auto& sinks_observer = url_to_sinks_observer_[source.id()];
  if (sinks_observer && sinks_observer->listener() == listener)
    return false;

  sinks_observer.reset(new PresentationMediaSinksObserver(
      router_, listener, source,
      GetLastCommittedURLForFrame(render_frame_host_id_)));

  if (!sinks_observer->Init()) {
    url_to_sinks_observer_.erase(source.id());
    listener->OnScreenAvailabilityChanged(ScreenAvailability::DISABLED);
    return false;
  }

  return true;
}

void PresentationFrame::RemoveScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source =
      MediaSource::ForPresentationUrl(listener->GetAvailabilityUrl());
  auto sinks_observer_it = url_to_sinks_observer_.find(source.id());
  if (sinks_observer_it != url_to_sinks_observer_.end() &&
      sinks_observer_it->second->listener() == listener) {
    url_to_sinks_observer_.erase(sinks_observer_it);
  }
}

bool PresentationFrame::HasScreenAvailabilityListenerForTest(
    const MediaSource::Id& source_id) const {
  return url_to_sinks_observer_.find(source_id) != url_to_sinks_observer_.end();
}

void PresentationFrame::Reset() {
  for (const auto& pid_route : presentation_id_to_route_) {
    if (pid_route.second.is_local_presentation()) {
      auto* local_presentation_manager =
          LocalPresentationManagerFactory::GetOrCreateForWebContents(
              web_contents_);
      local_presentation_manager->UnregisterLocalPresentationController(
          pid_route.first, render_frame_host_id_);
    } else {
      router_->DetachRoute(pid_route.second.media_route_id());
    }
  }

  presentation_id_to_route_.clear();
  url_to_sinks_observer_.clear();
  connection_state_subscriptions_.clear();
  browser_connection_proxies_.clear();
}

void PresentationFrame::AddPresentation(
    const PresentationInfo& presentation_info,
    const MediaRoute& route) {
  presentation_id_to_route_.emplace(presentation_info.id, route);
}

void PresentationFrame::ConnectToPresentation(
    const PresentationInfo& presentation_info,
    mojo::PendingRemote<PresentationConnection> controller_connection_remote,
    mojo::PendingReceiver<PresentationConnection>
        receiver_connection_receiver) {
  const auto pid_route_it =
      presentation_id_to_route_.find(presentation_info.id);

  if (pid_route_it == presentation_id_to_route_.end()) {
    DLOG(WARNING) << "No route for [presentation_id]: " << presentation_info.id;
    return;
  }

  if (pid_route_it->second.is_local_presentation()) {
    auto* local_presentation_manager =
        LocalPresentationManagerFactory::GetOrCreateForWebContents(
            web_contents_);
    local_presentation_manager->RegisterLocalPresentationController(
        presentation_info, render_frame_host_id_,
        std::move(controller_connection_remote),
        std::move(receiver_connection_receiver), pid_route_it->second);
  } else {
    DVLOG(2)
        << "Creating BrowserPresentationConnectionProxy for [presentation_id]: "
        << presentation_info.id;
    MediaRoute::Id route_id = pid_route_it->second.media_route_id();
    if (base::Contains(browser_connection_proxies_, route_id)) {
      DLOG(ERROR) << __func__
                  << "Already has a BrowserPresentationConnectionProxy for "
                  << "route: " << route_id;
      return;
    }

    auto* proxy = new BrowserPresentationConnectionProxy(
        router_, route_id, std::move(receiver_connection_receiver),
        std::move(controller_connection_remote));
    browser_connection_proxies_.emplace(route_id, base::WrapUnique(proxy));
  }
}

void PresentationFrame::RemovePresentation(const std::string& presentation_id) {
  // Remove the presentation id mapping so a later call to Reset is a no-op.
  auto it = presentation_id_to_route_.find(presentation_id);
  if (it == presentation_id_to_route_.end())
    return;

  auto route_id = it->second.media_route_id();
  presentation_id_to_route_.erase(presentation_id);
  browser_connection_proxies_.erase(route_id);
  // We keep the PresentationConnectionStateChangedCallback registered with MR
  // so the MRP can tell us when terminate() completed.
}

void PresentationFrame::ListenForConnectionStateChange(
    const PresentationInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto it = presentation_id_to_route_.find(connection.id);
  if (it == presentation_id_to_route_.end()) {
    DLOG(ERROR) << __func__
                << "route id not found for presentation: " << connection.id;
    return;
  }

  const MediaRoute::Id& route_id = it->second.media_route_id();
  if (connection_state_subscriptions_.find(route_id) !=
      connection_state_subscriptions_.end()) {
    DLOG(ERROR) << __func__
                << "Already listening connection state change for route: "
                << route_id;
    return;
  }

  connection_state_subscriptions_.emplace(
      route_id, router_->AddPresentationConnectionStateChangedCallback(
                    route_id, state_changed_cb));
}

StartPresentationContext::StartPresentationContext(
    const content::PresentationRequest& presentation_request,
    PresentationConnectionCallback success_cb,
    PresentationConnectionErrorCallback error_cb)
    : presentation_request_(presentation_request),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(success_cb_);
  DCHECK(error_cb_);
}

StartPresentationContext::~StartPresentationContext() {
  if (success_cb_ && error_cb_) {
    std::move(error_cb_).Run(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::UNKNOWN, "Unknown error."));
  }
}

void StartPresentationContext::InvokeSuccessCallback(
    const std::string& presentation_id,
    const GURL& presentation_url,
    const MediaRoute& route,
    mojom::RoutePresentationConnectionPtr connection) {
  if (success_cb_ && error_cb_) {
    std::move(success_cb_)
        .Run(blink::mojom::PresentationInfo(presentation_url, presentation_id),
             std::move(connection), route);
  }
}

void StartPresentationContext::InvokeErrorCallback(
    const blink::mojom::PresentationError& error) {
  if (success_cb_ && error_cb_) {
    std::move(error_cb_).Run(error);
  }
}

void StartPresentationContext::HandleRouteResponse(
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route()) {
    InvokeErrorCallback(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::UNKNOWN, result.error()));
  } else {
    InvokeSuccessCallback(result.presentation_id(), result.presentation_url(),
                          *result.route(), std::move(connection));
  }
}

PresentationServiceDelegateImpl*
PresentationServiceDelegateImpl::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  PresentationServiceDelegateImpl::CreateForWebContents(web_contents);
  return PresentationServiceDelegateImpl::FromWebContents(web_contents);
}

PresentationServiceDelegateImpl::PresentationServiceDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      router_(MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext())) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationServiceDelegateImpl::~PresentationServiceDelegateImpl() = default;

void PresentationServiceDelegateImpl::AddObserver(int render_process_id,
                                                  int render_frame_id,
                                                  DelegateObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(render_process_id, render_frame_id, observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(int render_process_id,
                                                     int render_frame_id) {
  observers_.RemoveObserver(render_process_id, render_frame_id);
}

bool PresentationServiceDelegateImpl::AddScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     render_frame_id);
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  return presentation_frame->SetScreenAvailabilityListener(listener);
}

void PresentationServiceDelegateImpl::RemoveScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemoveScreenAvailabilityListener(listener);
}

void PresentationServiceDelegateImpl::Reset(int render_process_id,
                                            int render_frame_id) {
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end()) {
    it->second->Reset();
    presentation_frames_.erase(it);
  }

  if (default_presentation_request_ &&
      render_frame_host_id ==
          default_presentation_request_->render_frame_host_id) {
    ClearDefaultPresentationRequest();
  }
}

PresentationFrame* PresentationServiceDelegateImpl::GetOrAddPresentationFrame(
    const content::GlobalFrameRoutingId& render_frame_host_id) {
  auto& presentation_frame = presentation_frames_[render_frame_host_id];
  if (!presentation_frame) {
    presentation_frame.reset(
        new PresentationFrame(render_frame_host_id, web_contents_, router_));
  }
  return presentation_frame.get();
}

void PresentationServiceDelegateImpl::SetDefaultPresentationUrls(
    const content::PresentationRequest& request,
    content::DefaultPresentationConnectionCallback callback) {
  if (request.presentation_urls.empty()) {
    ClearDefaultPresentationRequest();
    return;
  }

  DCHECK(!callback.is_null());
  default_presentation_started_callback_ = std::move(callback);
  default_presentation_request_ = request;
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationChanged(*default_presentation_request_);
}

void PresentationServiceDelegateImpl::OnJoinRouteResponse(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    const GURL& presentation_url,
    const std::string& presentation_id,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route()) {
    std::move(error_cb).Run(PresentationError(
        PresentationErrorType::NO_PRESENTATION_FOUND, result.error()));
  } else {
    DVLOG(1) << "OnJoinRouteResponse: "
             << "route_id: " << result.route()->media_route_id()
             << ", presentation URL: " << presentation_url
             << ", presentation ID: " << presentation_id;
    DCHECK_EQ(presentation_id, result.presentation_id());
    PresentationInfo presentation_info(presentation_url,
                                       result.presentation_id());
    AddPresentation(render_frame_host_id, presentation_info, *result.route());
    EnsurePresentationConnection(render_frame_host_id, presentation_info,
                                 &connection);
    std::move(success_cb)
        .Run(blink::mojom::PresentationConnectionResult::New(
            presentation_info.Clone(), std::move(connection->connection_remote),
            std::move(connection->connection_receiver)));
  }
}

void PresentationServiceDelegateImpl::OnStartPresentationSucceeded(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    content::PresentationConnectionCallback success_cb,
    const PresentationInfo& new_presentation_info,
    mojom::RoutePresentationConnectionPtr connection,
    const MediaRoute& route) {
  DVLOG(1) << "OnStartPresentationSucceeded: "
           << "route_id: " << route.media_route_id()
           << ", presentation URL: " << new_presentation_info.url
           << ", presentation ID: " << new_presentation_info.id;
  AddPresentation(render_frame_host_id, new_presentation_info, route);
  EnsurePresentationConnection(render_frame_host_id, new_presentation_info,
                               &connection);
  std::move(success_cb)
      .Run(blink::mojom::PresentationConnectionResult::New(
          new_presentation_info.Clone(),
          std::move(connection->connection_remote),
          std::move(connection->connection_receiver)));
}

void PresentationServiceDelegateImpl::AddPresentation(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    const PresentationInfo& presentation_info,
    const MediaRoute& route) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->AddPresentation(presentation_info, route);
}

void PresentationServiceDelegateImpl::RemovePresentation(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    const std::string& presentation_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemovePresentation(presentation_id);
}

void PresentationServiceDelegateImpl::StartPresentation(
    const content::PresentationRequest& request,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  const auto& render_frame_host_id = request.render_frame_host_id;
  const auto& presentation_urls = request.presentation_urls;
  if (presentation_urls.empty()) {
    std::move(error_cb).Run(PresentationError(
        PresentationErrorType::UNKNOWN, "Invalid presentation arguments."));
    return;
  }
  if (std::find_if_not(presentation_urls.begin(), presentation_urls.end(),
                       IsValidPresentationUrl) != presentation_urls.end()) {
    std::move(error_cb).Run(
        PresentationError(PresentationErrorType::NO_PRESENTATION_FOUND,
                          "Invalid presentation URL."));
    return;
  }
  auto presentation_context = std::make_unique<StartPresentationContext>(
      request,
      base::BindOnce(
          &PresentationServiceDelegateImpl::OnStartPresentationSucceeded,
          GetWeakPtr(), render_frame_host_id, std::move(success_cb)),
      std::move(error_cb));
  if (start_presentation_cb_) {
    start_presentation_cb_.Run(std::move(presentation_context));
    return;
  }
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents_);
  if (!controller->ShowMediaRouterDialogForPresentation(
          std::move(presentation_context))) {
    LOG(ERROR)
        << "StartPresentation failed: unable to create Media Router dialog.";
  }
}

void PresentationServiceDelegateImpl::ReconnectPresentation(
    const content::PresentationRequest& request,
    const std::string& presentation_id,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  DVLOG(2) << "PresentationServiceDelegateImpl::ReconnectPresentation";
  const auto& presentation_urls = request.presentation_urls;
  const auto& render_frame_host_id = request.render_frame_host_id;
  if (presentation_urls.empty()) {
    std::move(error_cb).Run(
        PresentationError(PresentationErrorType::NO_PRESENTATION_FOUND,
                          "Invalid presentation arguments."));
    return;
  }

#if !defined(OS_ANDROID)
  if (IsAutoJoinPresentationId(presentation_id) &&
      ShouldCancelAutoJoinForOrigin(request.frame_origin)) {
    std::move(error_cb).Run(
        PresentationError(PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
                          "Auto-join request cancelled by user preferences."));
    return;
  }
#endif  // !defined(OS_ANDROID)

  auto* local_presentation_manager =
      LocalPresentationManagerFactory::GetOrCreateForWebContents(web_contents_);
  // Check local presentation across frames.
  if (local_presentation_manager->IsLocalPresentation(presentation_id)) {
    auto* route = local_presentation_manager->GetRoute(presentation_id);

    if (!route) {
      LOG(WARNING) << "No route found for [presentation_id]: "
                   << presentation_id;
      return;
    }

    if (!base::Contains(presentation_urls, route->media_source().url())) {
      DVLOG(2) << "Presentation URLs do not match URL of current presentation:"
               << route->media_source().url();
      return;
    }

    auto result = RouteRequestResult::FromSuccess(*route, presentation_id);
    OnJoinRouteResponse(render_frame_host_id, presentation_urls[0],
                        presentation_id, std::move(success_cb),
                        std::move(error_cb),
                        mojom::RoutePresentationConnectionPtr(), *result);
  } else {
    // TODO(crbug.com/627655): Handle multiple URLs.
    const GURL& presentation_url = presentation_urls[0];
    bool incognito = web_contents_->GetBrowserContext()->IsOffTheRecord();
    router_->JoinRoute(
        MediaSource::ForPresentationUrl(presentation_url).id(), presentation_id,
        request.frame_origin, web_contents_,
        base::BindOnce(&PresentationServiceDelegateImpl::OnJoinRouteResponse,
                       GetWeakPtr(), render_frame_host_id, presentation_url,
                       presentation_id, std::move(success_cb),
                       std::move(error_cb)),
        base::TimeDelta(), incognito);
  }
}

void PresentationServiceDelegateImpl::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalFrameRoutingId rfh_id(render_process_id,
                                             render_frame_id);
  auto route_id = GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }

  auto* local_presentation_manager =
      LocalPresentationManagerFactory::GetOrCreateForWebContents(web_contents_);

  if (local_presentation_manager->IsLocalPresentation(presentation_id)) {
    local_presentation_manager->UnregisterLocalPresentationController(
        presentation_id, rfh_id);
  } else {
    router_->DetachRoute(route_id);
  }
  RemovePresentation(rfh_id, presentation_id);
  // TODO(mfoltz): close() should always succeed so there is no need to keep the
  // state_changed_cb around - remove it and fire the ChangeEvent on the
  // PresentationConnection in Blink.
}

void PresentationServiceDelegateImpl::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalFrameRoutingId rfh_id(render_process_id,
                                             render_frame_id);
  auto route_id = GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }
  router_->TerminateRoute(route_id);
  RemovePresentation(rfh_id, presentation_id);
}

void PresentationServiceDelegateImpl::ListenForConnectionStateChange(
    int render_process_id,
    int render_frame_id,
    const PresentationInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->ListenForConnectionStateChange(connection, state_changed_cb);
}

void PresentationServiceDelegateImpl::OnRouteResponse(
    const content::PresentationRequest& presentation_request,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route() || !base::Contains(presentation_request.presentation_urls,
                                         result.presentation_url())) {
    return;
  }

  PresentationInfo presentation_info(result.presentation_url(),
                                     result.presentation_id());
  AddPresentation(presentation_request.render_frame_host_id, presentation_info,
                  *result.route());
  if (default_presentation_request_ &&
      ArePresentationRequestsEqual(*default_presentation_request_,
                                   presentation_request)) {
    EnsurePresentationConnection(presentation_request.render_frame_host_id,
                                 presentation_info, &connection);
    default_presentation_started_callback_.Run(
        blink::mojom::PresentationConnectionResult::New(
            presentation_info.Clone(), std::move(connection->connection_remote),
            std::move(connection->connection_receiver)));
  } else {
    DCHECK(!connection);
  }
}

void PresentationServiceDelegateImpl::AddDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  default_presentation_request_observers_.AddObserver(observer);
}

void PresentationServiceDelegateImpl::RemoveDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  default_presentation_request_observers_.RemoveObserver(observer);
}

const content::PresentationRequest&
PresentationServiceDelegateImpl::GetDefaultPresentationRequest() const {
  DCHECK(HasDefaultPresentationRequest());
  return *default_presentation_request_;
}

bool PresentationServiceDelegateImpl::HasDefaultPresentationRequest() const {
  return !!default_presentation_request_;
}

base::WeakPtr<PresentationServiceDelegateImpl>
PresentationServiceDelegateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PresentationServiceDelegateImpl::HasScreenAvailabilityListenerForTest(
    int render_process_id,
    int render_frame_id,
    const MediaSource::Id& source_id) const {
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->HasScreenAvailabilityListenerForTest(source_id);
}

void PresentationServiceDelegateImpl::ClearDefaultPresentationRequest() {
  default_presentation_started_callback_.Reset();
  if (!default_presentation_request_)
    return;

  default_presentation_request_.reset();
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationRemoved();
}

std::unique_ptr<media::FlingingController>
PresentationServiceDelegateImpl::GetFlingingController(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalFrameRoutingId rfh_id(render_process_id,
                                             render_frame_id);
  MediaRoute::Id route_id = GetRouteId(rfh_id, presentation_id);

  if (route_id.empty())
    return nullptr;

  return router_->GetFlingingController(route_id);
}

MediaRoute::Id PresentationServiceDelegateImpl::GetRouteId(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    const std::string& presentation_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end()
             ? it->second->GetRouteId(presentation_id)
             : MediaRoute::Id();
}

#if !defined(OS_ANDROID)
bool PresentationServiceDelegateImpl::ShouldCancelAutoJoinForOrigin(
    const url::Origin& origin) const {
  const base::ListValue* origins =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs()
          ->GetList(prefs::kMediaRouterTabMirroringSources);
  return origins &&
         origins->Find(base::Value(origin.Serialize())) != origins->end();
}
#endif  // !defined(OS_ANDROID)

void PresentationServiceDelegateImpl::EnsurePresentationConnection(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    const PresentationInfo& presentation_info,
    mojom::RoutePresentationConnectionPtr* connection) {
  // This is where we ensure we have a valid Mojo pipe pair when starting or
  // joining a presentation.  If the MRP chose not return a pair of pipes
  // directly, we need to provide a BrowserPresentationConnectionProxy here or
  // connect to the LocalPresentationManager, as necessary.
  if (!*connection) {
    mojo::PendingRemote<PresentationConnection> controller_remote;
    mojo::PendingRemote<PresentationConnection> receiver_remote;
    *connection = mojom::RoutePresentationConnection::New(
        std::move(receiver_remote),
        controller_remote.InitWithNewPipeAndPassReceiver());
    auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
    presentation_frame->ConnectToPresentation(
        presentation_info, std::move(controller_remote),
        (*connection)->connection_remote.InitWithNewPipeAndPassReceiver());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PresentationServiceDelegateImpl)

}  // namespace media_router
