// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/cast_mirroring_service_host.h"
#include "chrome/browser/media/cast_remoting_connector.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_sink_service_status.h"
#include "chrome/browser/media/router/providers/cast/cast_media_route_provider.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/branded_strings.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/mirroring_media_controller_host_impl.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/openscreen_platform/network_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace media_router {
namespace {

const int kDefaultFrameTreeNodeId = -1;

constexpr char kLoggerComponent[] = "MediaRouterDesktop";

DesktopMediaPickerController::Params MakeDesktopPickerParams(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(web_contents);
#endif

  DesktopMediaPickerController::Params params(
      DesktopMediaPickerController::Params::RequestSource::kCast);
  // Value of `web_contents` comes from the UI, and typically corresponds to
  // the active tab.
  params.web_contents = web_contents;
  if (web_contents) {
    params.context = web_contents->GetTopLevelNativeWindow();
  }
  params.app_name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  params.target_name = params.app_name;
  params.select_only_screen = true;
  params.request_audio = true;
  params.force_audio_checkboxes_to_default_checked = true;

  return params;
}

// Returns a vector of media routes that are in `routes_a` and not in
// `routes_b`. Compares routes only by route id, and returns the version of
// the routes from `routes_a`.
std::vector<MediaRoute> GetRouteSetDifference(
    std::vector<MediaRoute> routes_a,
    std::vector<MediaRoute> routes_b) {
  std::vector<MediaRoute> routes;
  for (auto route_a : routes_a) {
    bool route_seen = false;
    for (auto route_b : routes_b) {
      if (route_a.media_route_id() == route_b.media_route_id()) {
        route_seen = true;
      }
    }

    if (!route_seen) {
      routes.emplace_back(route_a);
    }
  }

  return routes;
}

}  // namespace

MediaRouterDesktop::MediaRouterDesktop(content::BrowserContext* context)
    : context_(context),
      media_router_debugger_(context),
      cast_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      dial_provider_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MediaRouterDesktop::~MediaRouterDesktop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (media_sink_service_)
    media_sink_service_->RemoveLogger(GetLogger());
}

void MediaRouterDesktop::Initialize() {
  DCHECK(!internal_routes_observer_);
  media_sink_service_ = ShouldInitializeMediaRouteProviders()
                            ? DualMediaSinkService::GetInstance()
                            : nullptr;

  desktop_picker_ = std::make_unique<DesktopMediaPickerController>();

  // Because observer calls virtual methods on MediaRouter, it must be created
  // outside of the constructor.
  internal_routes_observer_ =
      std::make_unique<InternalMediaRoutesObserver>(this);
  if (media_sink_service_) {
    media_sink_service_->AddLogger(GetLogger());
    media_sink_service_->SetDiscoveryPermissionRejectedCallback(
        base::BindRepeating(
            &MediaRouterDesktop::OnLocalDiscoveryPermissionRejected,
            weak_factory_.GetWeakPtr()));
    InitializeMediaRouteProviders();
#if BUILDFLAG(IS_WIN)
    CanFirewallUseLocalPorts(
        base::BindOnce(&MediaRouterDesktop::OnFirewallCheckComplete,
                       weak_factory_.GetWeakPtr()));
#endif
  }
}

void MediaRouterDesktop::CreateRoute(const MediaSource::Id& source_id,
                                     const MediaSink::Id& sink_id,
                                     const url::Origin& origin,
                                     content::WebContents* web_contents,
                                     MediaRouteResponseCallback callback,
                                     base::TimeDelta timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);
  const MediaSink* sink = GetSinkById(sink_id);
  if (!sink) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Sink not found", mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    MediaRouterMetrics::RecordCreateRouteResultCode(result->result_code());
    std::move(callback).Run(nullptr, *result);
    return;
  }

  const MediaSource source(source_id);
  if (source.IsTabMirroringSource()) {
    // Ensure the CastRemotingConnector is created before mirroring starts.
    CastRemotingConnector* const connector =
        CastRemotingConnector::Get(web_contents);
    connector->ResetRemotingPermission();

    MediaRouterMojoMetrics::RecordTabMirroringMetrics(web_contents);
  }

  if (IsSiteInitiatedMirroringSource(source_id)) {
    MediaRouterMojoMetrics::RecordSiteInitiatedMirroringStarted(web_contents,
                                                                source);
  }

  MediaRouterMetrics::RecordMediaSinkType(sink->icon_type());
  // Record which of the possible ways the sink may render the source's
  // presentation URL (if it has one).
  if (source.url().is_valid()) {
    RecordPresentationRequestUrlBySink(source, sink->provider_id());
  }

  const mojom::MediaRouteProviderId provider_id = sink->provider_id();

  const std::string presentation_id = MediaRouterBase::CreatePresentationId();
  auto mr_callback = base::BindOnce(&MediaRouterDesktop::RouteResponseReceived,
                                    weak_factory_.GetWeakPtr(), presentation_id,
                                    provider_id, std::move(callback), false);

  if (source.IsDesktopMirroringSource()) {
    desktop_picker_->Show(
        MakeDesktopPickerParams(web_contents),
        {DesktopMediaList::Type::kScreen},
        base::BindRepeating([](content::WebContents* wc) { return true; }),
        base::BindOnce(&MediaRouterDesktop::CreateRouteWithSelectedDesktop,
                       weak_factory_.GetWeakPtr(), provider_id, sink_id,
                       presentation_id, origin, web_contents, timeout,
                       std::move(mr_callback)));
  } else {
    const content::FrameTreeNodeId frame_tree_node_id =
        web_contents ? web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId()
                     : content::FrameTreeNodeId();
    media_route_providers_[provider_id]->CreateRoute(
        source_id, sink_id, presentation_id, origin, frame_tree_node_id.value(),
        timeout, std::move(mr_callback));
  }
}

// TODO(crbug.com/1418747): The auto-join and/or "mirroring to flinging"
// features result in multiple presentations with identical presentation IDs
// of "auto-join".  This is a latent bug because the rest of the code assumes
// presentation IDs are unique.
void MediaRouterDesktop::JoinRoute(const MediaSource::Id& source_id,
                                   const std::string& presentation_id,
                                   const url::Origin& origin,
                                   content::WebContents* web_contents,
                                   MediaRouteResponseCallback callback,
                                   base::TimeDelta timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForPresentation(presentation_id);
  if (!provider_id || !HasJoinableRoute()) {
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Route not found", mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    MediaRouterMetrics::RecordJoinRouteResultCode(result->result_code());
    // TODO(btolsch): This should really move `result` now that there's only a
    // single callback.
    std::move(callback).Run(nullptr, *result);
    return;
  }

  const content::FrameTreeNodeId frame_tree_node_id =
      web_contents ? web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId()
                   : content::FrameTreeNodeId();
  auto mr_callback = base::BindOnce(&MediaRouterDesktop::RouteResponseReceived,
                                    weak_factory_.GetWeakPtr(), presentation_id,
                                    *provider_id, std::move(callback), true);
  media_route_providers_[*provider_id]->JoinRoute(
      source_id, presentation_id, origin, frame_tree_node_id.value(), timeout,
      std::move(mr_callback));
}

void MediaRouterDesktop::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    MediaRouterMetrics::RecordJoinRouteResultCode(
        mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }
  auto callback =
      base::BindOnce(&MediaRouterDesktop::OnTerminateRouteResult,
                     weak_factory_.GetWeakPtr(), route_id, *provider_id);
  media_route_providers_[*provider_id]->TerminateRoute(route_id,
                                                       std::move(callback));
}

void MediaRouterDesktop::DetachRoute(MediaRoute::Id route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    return;
  }
  media_route_providers_[*provider_id]->DetachRoute(route_id);
}

void MediaRouterDesktop::SendRouteMessage(const MediaRoute::Id& route_id,
                                          const std::string& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    return;
  }
  media_route_providers_[*provider_id]->SendRouteMessage(route_id, message);
}

void MediaRouterDesktop::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!provider_id) {
    return;
  }
  media_route_providers_[*provider_id]->SendRouteBinaryMessage(route_id, *data);
}

void MediaRouterDesktop::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!media_sink_service_) {
    return;
  }

  if (media_sink_service_->MdnsDiscoveryStarted() &&
      media_sink_service_->DialDiscoveryStarted()) {
    DiscoverSinksNow();
  } else {
    GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The user interacted with MR. Starting dial and mDNS discovery.", "",
        "", "");
    media_sink_service_->StartDiscovery();
  }

  if (!media_sink_service_subscription_) {
    media_sink_service_subscription_ =
        media_sink_service_->AddSinksDiscoveredCallback(
            base::BindRepeating(&MediaSinkServiceStatus::UpdateDiscoveredSinks,
                                media_sink_service_status_.GetWeakPtr()));
  }
}

std::vector<MediaRoute> MediaRouterDesktop::GetCurrentRoutes() const {
  return internal_routes_observer_->current_routes();
}

std::unique_ptr<media::FlingingController>
MediaRouterDesktop::GetFlingingController(const MediaRoute::Id& route_id) {
  return nullptr;
}

MirroringMediaControllerHost*
MediaRouterDesktop::GetMirroringMediaControllerHost(
    const MediaRoute::Id& route_id) {
  auto it = mirroring_media_controller_hosts_.find(route_id);
  if (it != mirroring_media_controller_hosts_.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

IssueManager* MediaRouterDesktop::GetIssueManager() {
  return &issue_manager_;
}

void MediaRouterDesktop::GetMediaController(
    const MediaRoute::Id& route_id,
    mojo::PendingReceiver<mojom::MediaController> controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  auto* route = GetRoute(route_id);
  std::optional<mojom::MediaRouteProviderId> provider_id =
      GetProviderIdForRoute(route_id);
  if (!route || !provider_id ||
      route->controller_type() == RouteControllerType::kNone) {
    return;
  }
  auto callback = base::BindOnce(&MediaRouterDesktop::OnMediaControllerBound,
                                 weak_factory_.GetWeakPtr(), route_id);
  media_route_providers_[*provider_id]->BindMediaController(
      route_id, std::move(controller), std::move(observer),
      std::move(callback));
}

base::Value MediaRouterDesktop::GetLogs() const {
  return logger_.GetLogsAsValue();
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

LoggerImpl* MediaRouterDesktop::GetLogger() {
  return &logger_;
}

MediaRouterDebugger& MediaRouterDesktop::GetDebugger() {
  return media_router_debugger_;
}

bool MediaRouterDesktop::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  // On Windows and macOS, where discovery might trigger a permission
  // prompt, do not start discovery service.
  if (media_sink_service_) {
    media_sink_service_->StartDiscovery();
    GetLogger()->LogInfo(mojom::LogCategory::kDiscovery, kLoggerComponent,
                         "Starting the DIAL and mDNS discovery because a media "
                         "sink is registered for the first time.",
                         "", "", "");
  }
#endif

  // Create an observer list for the media source and add `observer`
  // to it. Fail if `observer` is already registered.
  const MediaSource source = MediaSinksQuery::GetKey(*observer);
  std::unique_ptr<MediaSinksQuery>& sinks_query = sinks_queries_[source.id()];
  bool is_new_query = false;
  if (!sinks_query) {
    is_new_query = true;
    sinks_query = std::make_unique<MediaSinksQuery>();
  } else {
    DCHECK(!sinks_query->HasObserver(observer));
  }
  sinks_query->AddObserver(observer);

  // If the query isn't new, then there is no need to call MRPs.
  if (is_new_query) {
    for (const auto& provider : media_route_providers_) {
      // TODO(crbug.com/40133937): Don't allow MediaSource::ForAnyTab().id() to
      // be passed here.
      provider.second->StartObservingMediaSinks(source.id());
    }
  }
  return true;
}

void MediaRouterDesktop::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const MediaSource source = MediaSinksQuery::GetKey(*observer);
  auto it = sinks_queries_.find(source.id());
  if (it == sinks_queries_.end() || !it->second->HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // HasObservers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->RemoveObserver(observer);
  // Since all tabs share the tab sinks query, we don't want to delete it
  // here.
  if (!it->second->HasObservers() && !source.IsTabMirroringSource()) {
    for (const auto& provider : media_route_providers_) {
      // TODO(crbug.com/40133937): Don't allow MediaSource::ForAnyTab().id() to
      // be passed here.
      provider.second->StopObservingMediaSinks(source.id());
    }
    sinks_queries_.erase(source.id());
  }
}

void MediaRouterDesktop::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const bool is_first_observer = !routes_query_.HasObservers();
  if (!is_first_observer) {
    DCHECK(!routes_query_.HasObserver(observer));
  }

  routes_query_.AddObserver(observer);
  if (is_first_observer) {
    for (const auto& provider : media_route_providers_) {
      provider.second->StartObservingMediaRoutes();
      // The MRPs will call MediaRouterDesktop::OnRoutesUpdated() soon, if
      // there are any existing routes the new observer should be aware of.
    }
  } else {
    // Return to the event loop before notifying of a cached route list because
    // MediaRoutesObserver is calling this method from its constructor, and that
    // must complete before invoking its virtual OnRoutesUpdated() method.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaRouterDesktop::NotifyNewObserversOfExistingRoutes,
                       weak_factory_.GetWeakPtr()));
  }
}

void MediaRouterDesktop::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  routes_query_.RemoveObserver(observer);
}

void MediaRouterDesktop::RegisterPresentationConnectionMessageObserver(
    PresentationConnectionMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  const MediaRoute::Id& route_id = observer->route_id();
  auto& observer_list = message_observers_[route_id];
  if (!observer_list) {
    observer_list =
        std::make_unique<PresentationConnectionMessageObserverList>();
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }
  observer_list->AddObserver(observer);
}

void MediaRouterDesktop::UnregisterPresentationConnectionMessageObserver(
    PresentationConnectionMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);

  const MediaRoute::Id& route_id = observer->route_id();
  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end() || !it->second->HasObserver(observer)) {
    return;
  }

  it->second->RemoveObserver(observer);
}

void MediaRouterDesktop::RegisterMediaRouteProvider(
    mojom::MediaRouteProviderId provider_id,
    mojo::PendingRemote<mojom::MediaRouteProvider>
        media_route_provider_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!base::Contains(media_route_providers_, provider_id));
  mojo::Remote<mojom::MediaRouteProvider> bound_remote(
      std::move(media_route_provider_remote));
  bound_remote.set_disconnect_handler(
      base::BindOnce(&MediaRouterDesktop::OnProviderConnectionError,
                     weak_factory_.GetWeakPtr(), provider_id));
  media_route_providers_[provider_id] = std::move(bound_remote);
}

void MediaRouterDesktop::OnSinksReceived(
    mojom::MediaRouteProviderId provider_id,
    const std::string& media_source,
    const std::vector<MediaSinkInternal>& internal_sinks,
    const std::vector<url::Origin>& origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  media_sink_service_status_.UpdateAvailableSinks(provider_id, media_source,
                                                  internal_sinks);
  auto it = sinks_queries_.find(MediaSinksQuery::GetKey(media_source).id());
  if (it == sinks_queries_.end()) {
    return;
  }

  std::vector<MediaSink> sinks;
  sinks.reserve(internal_sinks.size());
  for (const auto& internal_sink : internal_sinks) {
    sinks.push_back(internal_sink.sink());
  }

  auto* sinks_query = it->second.get();
  sinks_query->SetSinksForProvider(provider_id, sinks);
  sinks_query->set_origins(origins);
  sinks_query->NotifyObservers();
}

void MediaRouterDesktop::OnIssue(const IssueInfo& issue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetIssueManager()->AddIssue(issue);
}

void MediaRouterDesktop::ClearTopIssueForSink(const MediaSink::Id& sink_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetIssueManager()->ClearTopIssueForSink(sink_id);
}

void MediaRouterDesktop::OnRoutesUpdated(
    mojom::MediaRouteProviderId provider_id,
    const std::vector<MediaRoute>& routes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto current_routes = GetCurrentRoutes();
  std::vector<MediaRoute> added_routes =
      GetRouteSetDifference(routes, current_routes);
  std::vector<MediaRoute> removed_routes =
      GetRouteSetDifference(current_routes, routes);

  // Update the internal_routes_observer_, and SetRoutesForProvider before
  // AddMirroringMediaControllerHost, since the latter relies on these to be up
  // to date.
  internal_routes_observer_->OnRoutesUpdated(routes);
  routes_query_.SetRoutesForProvider(provider_id, routes);

  for (const auto& route : added_routes) {
    if (route.IsLocalMirroringRoute()) {
      AddMirroringMediaControllerHost(route);
    }
  }
  for (const auto& route : removed_routes) {
    mirroring_media_controller_hosts_.erase(route.media_route_id());
  }

  routes_query_.NotifyObservers();
}

void MediaRouterDesktop::OnPresentationConnectionStateChanged(
    const std::string& route_id,
    blink::mojom::PresentationConnectionState state) {
  NotifyPresentationConnectionStateChange(route_id, state);
}

void MediaRouterDesktop::OnPresentationConnectionClosed(
    const std::string& route_id,
    blink::mojom::PresentationConnectionCloseReason reason,
    const std::string& message) {
  NotifyPresentationConnectionClose(route_id, reason, message);
}

void MediaRouterDesktop::OnRouteMessagesReceived(
    const std::string& route_id,
    std::vector<mojom::RouteMessagePtr> messages) {
  if (messages.empty()) {
    return;
  }

  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end()) {
    return;
  }

  for (auto& observer : *it->second) {
    // TODO(mfoltz): We have to clone the messages here in case there are
    // multiple observers.  This can be removed once we stop passing messages
    // through the MR and use the PresentationConnectionPtr directly.
    std::vector<mojom::RouteMessagePtr> messages_copy;
    for (auto& message : messages) {
      messages_copy.emplace_back(message->Clone());
    }

    observer.OnMessagesReceived(std::move(messages_copy));
  }
}

void MediaRouterDesktop::GetMediaSinkServiceStatus(
    mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) {
  std::move(callback).Run(media_sink_service_status_.GetStatusAsJSONString());
}

void MediaRouterDesktop::GetLogger(
    mojo::PendingReceiver<mojom::Logger> receiver) {
  logger_.BindReceiver(std::move(receiver));
}

void MediaRouterDesktop::GetDebugger(
    mojo::PendingReceiver<mojom::Debugger> receiver) {
  media_router_debugger_.BindReceiver(std::move(receiver));
}

void MediaRouterDesktop::GetLogsAsString(GetLogsAsStringCallback callback) {
  std::move(callback).Run(logger_.GetLogsAsJson());
}

void MediaRouterDesktop::Shutdown() {
  // The observer calls virtual methods on MediaRouter; it must be destroyed
  // outside of the dtor
  internal_routes_observer_.reset();
}

void MediaRouterDesktop::OnTerminateRouteResult(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProviderId provider_id,
    const std::optional<std::string>& error_text,
    mojom::RouteRequestResultCode result_code) {
  MediaRouterMetrics::RecordMediaRouteProviderTerminateRoute(result_code,
                                                             provider_id);
}

void MediaRouterDesktop::OnRouteAdded(mojom::MediaRouteProviderId provider_id,
                                      const MediaRoute& route) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  routes_query_.AddRouteForProvider(provider_id, route);
  routes_query_.NotifyObservers();
}

void MediaRouterDesktop::RouteResponseReceived(
    const std::string& presentation_id,
    mojom::MediaRouteProviderId provider_id,
    MediaRouteResponseCallback callback,
    bool is_join,
    const std::optional<MediaRoute>& media_route,
    mojom::RoutePresentationConnectionPtr connection,
    const std::optional<std::string>& error_text,
    mojom::RouteRequestResultCode result_code) {
  DCHECK(!connection ||
         (connection->connection_remote && connection->connection_receiver));
  std::unique_ptr<RouteRequestResult> result;
  if (!media_route) {
    // An error occurred.
    const std::string& error = (error_text && !error_text->empty())
                                   ? *error_text
                                   : std::string("Unknown error.");
    result = RouteRequestResult::FromError(error, result_code);
  } else {
    result = RouteRequestResult::FromSuccess(*media_route, presentation_id);
    OnRouteAdded(provider_id, *media_route);
  }

  if (is_join) {
    MediaRouterMetrics::RecordJoinRouteResultCode(result->result_code(),
                                                  provider_id);
  } else {
    MediaRouterMetrics::RecordCreateRouteResultCode(result->result_code(),
                                                    provider_id);
  }

  std::move(callback).Run(std::move(connection), *result);
}

void MediaRouterDesktop::OnLocalDiscoveryPermissionRejected() {
  if (base::FeatureList::IsEnabled(kShowCastPermissionRejectedError)) {
    GetIssueManager()->AddPermissionRejectedIssue();
  }
}

void MediaRouterDesktop::OnMediaControllerBound(const MediaRoute::Id& route_id,
                                                bool success) {
  MediaRouterMojoMetrics::RecordMediaRouteControllerCreationResult(success);
}

void MediaRouterDesktop::AddMirroringMediaControllerHost(
    const MediaRoute& route) {
  mojo::Remote<media_router::mojom::MediaController> controller_remote;
  mojo::PendingReceiver<media_router::mojom::MediaController>
      controller_receiver = controller_remote.BindNewPipeAndPassReceiver();
  auto host = std::make_unique<MirroringMediaControllerHostImpl>(
      std::move(controller_remote));
  auto observer_remote = host->GetMediaStatusObserverPendingRemote();
  GetMediaController(route.media_route_id(), std::move(controller_receiver),
                     std::move(observer_remote));
  mirroring_media_controller_hosts_[route.media_route_id()] = std::move(host);
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
  MediaRouterDesktop::BindToMojoReceiver(
      media_router_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::MediaRouteProvider> wired_display_provider_remote;
  wired_display_provider_ = std::make_unique<WiredDisplayMediaRouteProvider>(
      wired_display_provider_remote.InitWithNewPipeAndPassReceiver(),
      std::move(media_router_remote), Profile::FromBrowserContext(context()));
  RegisterMediaRouteProvider(mojom::MediaRouteProviderId::WIRED_DISPLAY,
                             std::move(wired_display_provider_remote));
}

void MediaRouterDesktop::InitializeCastMediaRouteProvider() {
  DCHECK(media_sink_service_);
  auto task_runner =
      cast_channel::CastSocketService::GetInstance()->task_runner();
  mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
  MediaRouterDesktop::BindToMojoReceiver(
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
  MediaRouterDesktop::BindToMojoReceiver(
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

std::string MediaRouterDesktop::GetHashToken() {
  return GetReceiverIdHashToken(
      Profile::FromBrowserContext(context())->GetPrefs());
}

void MediaRouterDesktop::DiscoverSinksNow() {
  media_sink_service_->DiscoverSinksNow();
  for (const auto& provider : media_route_providers_) {
    provider.second->DiscoverSinksNow();
  }
}

void MediaRouterDesktop::OnProviderConnectionError(
    mojom::MediaRouteProviderId provider_id) {
  media_route_providers_.erase(provider_id);
}

void MediaRouterDesktop::BindToMojoReceiver(
    mojo::PendingReceiver<mojom::MediaRouter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaRouterDesktop::CreateRouteWithSelectedDesktop(
    mojom::MediaRouteProviderId provider_id,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    content::WebContents* web_contents,
    base::TimeDelta timeout,
    mojom::MediaRouteProvider::CreateRouteCallback mr_callback,
    const std::string& err,
    content::DesktopMediaID media_id) {
  if (!err.empty()) {
    std::move(mr_callback)
        .Run(std::nullopt, nullptr, err,
             mojom::RouteRequestResultCode::DESKTOP_PICKER_FAILED);
    return;
  }

  if (media_id.is_null()) {
    std::move(mr_callback)
        .Run(std::nullopt, nullptr, "User canceled capture dialog",
             mojom::RouteRequestResultCode::CANCELLED);
    return;
  }

  media_route_providers_[provider_id]->CreateRoute(
      MediaSource::ForDesktop(media_id.ToString(), media_id.audio_share).id(),
      sink_id, presentation_id, origin, kDefaultFrameTreeNodeId, timeout,
      base::BindOnce(
          [](mojom::MediaRouteProvider::CreateRouteCallback inner_callback,
             const std::optional<media_router::MediaRoute>& route,
             mojom::RoutePresentationConnectionPtr connection,
             const std::optional<std::string>& error_text,
             mojom::RouteRequestResultCode result_code) {
            std::move(inner_callback)
                .Run(route, std::move(connection), error_text, result_code);
          },
          std::move(mr_callback)));
}

std::optional<mojom::MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForPresentation(
    const std::string& presentation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (presentation_id == kAutoJoinPresentationId ||
      base::StartsWith(presentation_id, kCastPresentationIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    return mojom::MediaRouteProviderId::CAST;
  }
  for (const auto& provider_to_routes : routes_query_.providers_to_routes()) {
    const mojom::MediaRouteProviderId provider_id = provider_to_routes.first;
    const std::vector<MediaRoute>& routes = provider_to_routes.second;
    DCHECK_LE(base::ranges::count(routes, presentation_id,
                                  &MediaRoute::presentation_id),
              1);
    if (base::Contains(routes, presentation_id, &MediaRoute::presentation_id)) {
      return provider_id;
    }
  }
  return std::nullopt;
}

std::optional<mojom::MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& provider_to_routes : routes_query_.providers_to_routes()) {
    const mojom::MediaRouteProviderId provider_id = provider_to_routes.first;
    const std::vector<MediaRoute>& routes = provider_to_routes.second;
    if (base::Contains(routes, route_id, &MediaRoute::media_route_id)) {
      return provider_id;
    }
  }
  return std::nullopt;
}

std::optional<mojom::MediaRouteProviderId>
MediaRouterDesktop::GetProviderIdForSink(const MediaSink::Id& sink_id) {
  const MediaSink* sink = GetSinkById(sink_id);
  return sink ? std::make_optional<mojom::MediaRouteProviderId>(
                    sink->provider_id())
              : std::nullopt;
}

const MediaSink* MediaRouterDesktop::GetSinkById(
    const MediaSink::Id& sink_id) const {
  // TODO(takumif): It is inefficient to iterate through all the sinks queries,
  // so there should be one list containing all the sinks.
  for (const auto& sinks_query : sinks_queries_) {
    const std::vector<MediaSink>& sinks =
        sinks_query.second->cached_sink_list();
    DCHECK_LE(base::ranges::count(sinks, sink_id, &MediaSink::id), 1);
    auto sink_it = base::ranges::find(sinks, sink_id, &MediaSink::id);
    if (sink_it != sinks.end()) {
      return &(*sink_it);
    }
  }
  return nullptr;
}

const MediaRoute* MediaRouterDesktop::GetRoute(
    const MediaRoute::Id& route_id) const {
  const auto& routes = internal_routes_observer_->current_routes();
  auto it = base::ranges::find(routes, route_id, &MediaRoute::media_route_id);
  return it == routes.end() ? nullptr : &*it;
}

void MediaRouterDesktop::NotifyNewObserversOfExistingRoutes() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  routes_query_.NotifyNewObserversOfExistingRoutes();
}

// NOTE: To record this on Android, will need to move to
// //components/media_router and refactor to avoid the extensions dependency.
void MediaRouterDesktop::RecordPresentationRequestUrlBySink(
    const MediaSource& source,
    mojom::MediaRouteProviderId provider_id) {
  PresentationUrlBySink value = PresentationUrlBySink::kUnknown;
  // URLs that can be rendered in offscreen tabs (for cloud or Chromecast
  // sinks), or on a wired display.
  bool is_normal_url = source.url().SchemeIs(url::kHttpsScheme) ||
#if BUILDFLAG(ENABLE_EXTENSIONS)
                       source.url().SchemeIs(extensions::kExtensionScheme) ||
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
                       source.url().SchemeIs(url::kFileScheme);
  switch (provider_id) {
    case mojom::MediaRouteProviderId::WIRED_DISPLAY:
      if (is_normal_url) {
        value = PresentationUrlBySink::kNormalUrlToWiredDisplay;
      }
      break;
    case mojom::MediaRouteProviderId::CAST:
      if (source.IsCastPresentationUrl()) {
        value = PresentationUrlBySink::kCastUrlToChromecast;
      } else if (source.IsRemotePlaybackSource()) {
        value = PresentationUrlBySink::kRemotePlayback;
      } else if (is_normal_url) {
        value = PresentationUrlBySink::kNormalUrlToChromecast;
      }
      break;
    case mojom::MediaRouteProviderId::DIAL:
      if (source.IsDialSource()) {
        value = PresentationUrlBySink::kDialUrlToDial;
      }
      break;
    case mojom::MediaRouteProviderId::ANDROID_CAF:
    case mojom::MediaRouteProviderId::TEST:
      break;
  }
  base::UmaHistogramEnumeration("MediaRouter.PresentationRequest.UrlBySink2",
                                value);
}

bool MediaRouterDesktop::HasJoinableRoute() const {
  return !(internal_routes_observer_->current_routes().empty());
}

bool MediaRouterDesktop::ShouldInitializeMediaRouteProviders() const {
  return !(disable_media_route_providers_for_test_ ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               kDisableMediaRouteProvidersForTestSwitch));
}

MediaRouterDesktop::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterDesktop::MediaSinksQuery::~MediaSinksQuery() = default;

// static
MediaSource MediaRouterDesktop::MediaSinksQuery::GetKey(
    const MediaSource::Id& id) {
  MediaSource source(id);
  if (source.IsTabMirroringSource()) {
    return MediaSource::ForAnyTab();
  }
  return source;
}

// static
MediaSource MediaRouterDesktop::MediaSinksQuery::GetKey(
    const MediaSinksObserver& observer) {
  if (!observer.source()) {
    return MediaSource{""};
  }
  return GetKey(observer.source()->id());
}

void MediaRouterDesktop::MediaSinksQuery::SetSinksForProvider(
    mojom::MediaRouteProviderId provider_id,
    const std::vector<MediaSink>& sinks) {
  std::erase_if(cached_sink_list_, [&provider_id](const MediaSink& sink) {
    return sink.provider_id() == provider_id;
  });
  cached_sink_list_.insert(cached_sink_list_.end(), sinks.begin(), sinks.end());
}

void MediaRouterDesktop::MediaSinksQuery::Reset() {
  cached_sink_list_.clear();
  origins_.clear();
}

void MediaRouterDesktop::MediaSinksQuery::AddObserver(
    MediaSinksObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnSinksUpdated(cached_sink_list_, origins_);
}

void MediaRouterDesktop::MediaSinksQuery::RemoveObserver(
    MediaSinksObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterDesktop::MediaSinksQuery::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnSinksUpdated(cached_sink_list_, origins_);
  }
}

bool MediaRouterDesktop::MediaSinksQuery::HasObserver(
    MediaSinksObserver* observer) const {
  return observers_.HasObserver(observer);
}

bool MediaRouterDesktop::MediaSinksQuery::HasObservers() const {
  return !observers_.empty();
}

MediaRouterDesktop::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterDesktop::MediaRoutesQuery::~MediaRoutesQuery() = default;

void MediaRouterDesktop::MediaRoutesQuery::SetRoutesForProvider(
    mojom::MediaRouteProviderId provider_id,
    const std::vector<MediaRoute>& routes) {
  providers_to_routes_[provider_id] = routes;
  UpdateCachedRouteList();
}

bool MediaRouterDesktop::MediaRoutesQuery::AddRouteForProvider(
    mojom::MediaRouteProviderId provider_id,
    const MediaRoute& route) {
  std::vector<MediaRoute>& routes = providers_to_routes_[provider_id];
  if (!base::Contains(routes, route.media_route_id(),
                      &MediaRoute::media_route_id)) {
    routes.push_back(route);
    UpdateCachedRouteList();
    return true;
  }
  return false;
}

void MediaRouterDesktop::MediaRoutesQuery::UpdateCachedRouteList() {
  cached_route_list_.emplace();
  for (const auto& provider_to_routes : providers_to_routes_) {
    cached_route_list_->insert(cached_route_list_->end(),
                               provider_to_routes.second.begin(),
                               provider_to_routes.second.end());
  }
}

void MediaRouterDesktop::MediaRoutesQuery::AddObserver(
    MediaRoutesObserver* observer) {
  new_observers_.push_back(observer);
  observers_.AddObserver(observer);
  observer->OnRoutesUpdated(
      cached_route_list_.value_or(std::vector<MediaRoute>()));
}

void MediaRouterDesktop::MediaRoutesQuery::RemoveObserver(
    MediaRoutesObserver* observer) {
  std::erase(new_observers_, observer);
  observers_.RemoveObserver(observer);
}

void MediaRouterDesktop::MediaRoutesQuery::NotifyObservers() {
  new_observers_.clear();
  for (auto& observer : observers_) {
    observer.OnRoutesUpdated(
        cached_route_list_.value_or(std::vector<MediaRoute>()));
  }
}

bool MediaRouterDesktop::MediaRoutesQuery::HasObserver(
    MediaRoutesObserver* observer) const {
  return observers_.HasObserver(observer);
}

bool MediaRouterDesktop::MediaRoutesQuery::HasObservers() const {
  return !observers_.empty();
}

void MediaRouterDesktop::MediaRoutesQuery::
    NotifyNewObserversOfExistingRoutes() {
  while (!new_observers_.empty()) {
    auto observer = new_observers_.back();
    std::erase(new_observers_, observer);
    observer->OnRoutesUpdated(
        cached_route_list().value_or(std::vector<MediaRoute>{}));
  }
}

MediaRouterDesktop::InternalMediaRoutesObserver::InternalMediaRoutesObserver(
    media_router::MediaRouter* router)
    : MediaRoutesObserver(router) {}

MediaRouterDesktop::InternalMediaRoutesObserver::
    ~InternalMediaRoutesObserver() = default;

void MediaRouterDesktop::InternalMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  current_routes_ = routes;
}

const std::vector<MediaRoute>&
MediaRouterDesktop::InternalMediaRoutesObserver::current_routes() const {
  return current_routes_;
}

}  // namespace media_router
