// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/cast_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

using media_router::MediaRoute;
using protocol::Response;
using protocol::Cast::Sink;

namespace {

media_router::MediaRouter* GetMediaRouter(content::WebContents* web_contents) {
  return media_router::MediaRouterFactory::GetApiForBrowserContext(
      web_contents->GetBrowserContext());
}

}  // namespace

class CastHandler::MediaRoutesObserver
    : public media_router::MediaRoutesObserver {
 public:
  MediaRoutesObserver(media_router::MediaRouter* router,
                      base::RepeatingClosure update_callback)
      : media_router::MediaRoutesObserver(router),
        update_callback_(std::move(update_callback)) {}
  ~MediaRoutesObserver() override = default;

  const std::vector<MediaRoute>& routes() const { return routes_; }

 private:
  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override {
    routes_ = routes;
    update_callback_.Run();
  }

  std::vector<MediaRoute> routes_;
  base::RepeatingClosure update_callback_;
};

class CastHandler::IssuesObserver : public media_router::IssuesObserver {
 public:
  IssuesObserver(
      media_router::MediaRouter* router,
      base::RepeatingCallback<void(const std::string& issue)> update_callback)
      : media_router::IssuesObserver(router->GetIssueManager()),
        update_callback_(std::move(update_callback)) {
    Init();
  }
  ~IssuesObserver() override = default;

  void OnIssue(const media_router::Issue& issue) override {
    update_callback_.Run(issue.info().title);
  }
  void OnIssuesCleared() override { update_callback_.Run(std::string()); }

 private:
  base::RepeatingCallback<void(const std::string& issue)> update_callback_;
};

CastHandler::CastHandler(content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher)
    : CastHandler(web_contents) {
  frontend_ = std::make_unique<protocol::Cast::Frontend>(dispatcher->channel());
  protocol::Cast::Dispatcher::wire(dispatcher, this);
}

CastHandler::~CastHandler() = default;

Response CastHandler::SetSinkToUse(const std::string& in_sink_name) {
  EnsureInitialized();
  media_router::PresentationServiceDelegateImpl::GetOrCreateForWebContents(
      web_contents_)
      ->set_start_presentation_cb(
          base::BindRepeating(&CastHandler::StartPresentation,
                              weak_factory_.GetWeakPtr(), in_sink_name));
  return Response::OK();
}

void CastHandler::StartTabMirroring(
    const std::string& in_sink_name,
    std::unique_ptr<StartTabMirroringCallback> callback) {
  EnsureInitialized();
  const media_router::MediaSink::Id& sink_id = GetSinkIdByName(in_sink_name);
  if (sink_id.empty()) {
    callback->sendFailure(Response::Error("Sink not found"));
    return;
  }

  router_->CreateRoute(
      query_result_manager_
          ->GetSourceForCastModeAndSink(media_router::MediaCastMode::TAB_MIRROR,
                                        sink_id)
          ->id(),
      sink_id, url::Origin::Create(GURL()), web_contents_,
      base::BindOnce(&CastHandler::OnTabMirroringStarted,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      media_router::GetRouteRequestTimeout(
          media_router::MediaCastMode::TAB_MIRROR),
      web_contents_->GetBrowserContext()->IsOffTheRecord());
}

Response CastHandler::StopCasting(const std::string& in_sink_name) {
  EnsureInitialized();
  const media_router::MediaSink::Id& sink_id = GetSinkIdByName(in_sink_name);
  if (sink_id.empty())
    return Response::Error("Sink not found");
  const MediaRoute::Id& route_id = GetRouteIdForSink(sink_id);
  if (route_id.empty())
    return Response::Error("Route not found");
  router_->TerminateRoute(route_id);
  initiated_routes_.erase(route_id);
  return Response::OK();
}

Response CastHandler::Enable(protocol::Maybe<std::string> in_presentation_url) {
  EnsureInitialized();
  StartObservingForSinks(std::move(in_presentation_url));
  return Response::OK();
}

Response CastHandler::Disable() {
  query_result_manager_.reset();
  routes_observer_.reset();
  issues_observer_.reset();
  for (const MediaRoute::Id& route_id : initiated_routes_)
    router_->TerminateRoute(route_id);
  return Response::OK();
}

void CastHandler::OnResultsUpdated(
    const std::vector<media_router::MediaSinkWithCastModes>& sinks) {
  sinks_ = sinks;
  SendSinkUpdate();
}

CastHandler::CastHandler(content::WebContents* web_contents)
    : web_contents_(web_contents), router_(GetMediaRouter(web_contents)) {}

void CastHandler::EnsureInitialized() {
  if (query_result_manager_)
    return;

  query_result_manager_ =
      std::make_unique<media_router::QueryResultManager>(router_);
  query_result_manager_->AddObserver(this);
  routes_observer_ = std::make_unique<MediaRoutesObserver>(
      router_, base::BindRepeating(&CastHandler::SendSinkUpdate,
                                   base::Unretained(this)));
  issues_observer_ = std::make_unique<IssuesObserver>(
      router_,
      base::BindRepeating(&CastHandler::OnIssue, base::Unretained(this)));
}

void CastHandler::StartPresentation(
    const std::string& sink_name,
    std::unique_ptr<media_router::StartPresentationContext> context) {
  url::Origin frame_origin = context->presentation_request().frame_origin;
  std::vector<media_router::MediaSource> sources;
  for (const auto& url : context->presentation_request().presentation_urls)
    sources.push_back(media_router::MediaSource::ForPresentationUrl(url));
  query_result_manager_->SetSourcesForCastMode(
      media_router::MediaCastMode::PRESENTATION, sources, frame_origin);
  const media_router::MediaSink::Id& sink_id = GetSinkIdByName(sink_name);

  // TODO(takumif): This assumes that Media Router has sink-source compatibility
  // cached, and can respond to |query_result_manager_| synchronously. If it is
  // not cached, we must wait until compatibility update to call CreateRoute().
  router_->CreateRoute(
      query_result_manager_
          ->GetSourceForCastModeAndSink(
              media_router::MediaCastMode::PRESENTATION, sink_id)
          ->id(),
      sink_id, frame_origin, web_contents_,
      base::BindOnce(&CastHandler::OnPresentationStarted,
                     weak_factory_.GetWeakPtr(), std::move(context)),
      media_router::GetRouteRequestTimeout(
          media_router::MediaCastMode::PRESENTATION),
      web_contents_->GetBrowserContext()->IsOffTheRecord());
}

media_router::MediaSink::Id CastHandler::GetSinkIdByName(
    const std::string& sink_name) const {
  auto it = std::find_if(
      sinks_.begin(), sinks_.end(),
      [&sink_name](const media_router::MediaSinkWithCastModes& sink) {
        return sink.sink.name() == sink_name;
      });
  return it == sinks_.end() ? media_router::MediaSink::Id() : it->sink.id();
}

MediaRoute::Id CastHandler::GetRouteIdForSink(
    const media_router::MediaSink::Id& sink_id) const {
  const auto& routes = routes_observer_->routes();
  auto it = std::find_if(routes.begin(), routes.end(),
                         [&sink_id](const MediaRoute& route) {
                           return route.media_sink_id() == sink_id;
                         });
  return it == routes.end() ? MediaRoute::Id() : it->media_route_id();
}

void CastHandler::StartObservingForSinks(
    protocol::Maybe<std::string> presentation_url) {
  media_router::MediaSource mirroring_source(media_router::MediaSource::ForTab(
      SessionTabHelper::IdForTab(web_contents_).id()));
  query_result_manager_->SetSourcesForCastMode(
      media_router::MediaCastMode::TAB_MIRROR, {mirroring_source},
      url::Origin::Create(GURL()));

  if (presentation_url.isJust()) {
    url::Origin frame_origin =
        web_contents_->GetMainFrame()->GetLastCommittedOrigin();
    std::vector<media_router::MediaSource> sources = {
        media_router::MediaSource(presentation_url.fromJust())};
    query_result_manager_->SetSourcesForCastMode(
        media_router::MediaCastMode::PRESENTATION, sources, frame_origin);
  }
}

void CastHandler::SendSinkUpdate() {
  if (!frontend_)
    return;

  auto protocol_sinks = std::make_unique<protocol::Array<Sink>>();
  for (const media_router::MediaSinkWithCastModes& sink_with_modes : sinks_) {
    auto route_it = std::find_if(
        routes_observer_->routes().begin(), routes_observer_->routes().end(),
        [&sink_with_modes](const MediaRoute& route) {
          return route.media_sink_id() == sink_with_modes.sink.id();
        });
    std::string session = route_it == routes_observer_->routes().end()
                              ? ""
                              : route_it->description();
    std::unique_ptr<Sink> sink = Sink::Create()
                                     .SetName(sink_with_modes.sink.name())
                                     .SetId(sink_with_modes.sink.id())
                                     .Build();
    if (!session.empty())
      sink->SetSession(session);

    protocol_sinks->emplace_back(std::move(sink));
  }
  frontend_->SinksUpdated(std::move(protocol_sinks));
}

void CastHandler::OnTabMirroringStarted(
    std::unique_ptr<StartTabMirroringCallback> callback,
    media_router::mojom::RoutePresentationConnectionPtr connection,
    const media_router::RouteRequestResult& result) {
  if (result.result_code() == media_router::RouteRequestResult::OK) {
    initiated_routes_.insert(result.route()->media_route_id());
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(result.error()));
  }
}

void CastHandler::OnPresentationStarted(
    std::unique_ptr<media_router::StartPresentationContext> context,
    media_router::mojom::RoutePresentationConnectionPtr connection,
    const media_router::RouteRequestResult& result) {
  if (result.result_code() == media_router::RouteRequestResult::OK)
    initiated_routes_.insert(result.route()->media_route_id());
  context->HandleRouteResponse(std::move(connection), result);
}

void CastHandler::OnIssue(const std::string& issue) {
  if (frontend_)
    frontend_->IssueUpdated(issue);
}
