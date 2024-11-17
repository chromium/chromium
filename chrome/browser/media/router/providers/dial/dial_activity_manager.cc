// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"

#include <optional>
#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"
#include "components/media_router/common/media_source.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace media_router {

namespace {

// Returns the URL to use to launch |app_name| on |sink|.
GURL GetAppURL(const MediaSinkInternal& sink, const std::string& app_name) {
  // The DIAL spec (Section 5.4) implies that the app URL must not have a
  // trailing slash.
  return GURL(sink.dial_data().app_url.spec() + "/" + app_name);
}

// Returns the Application Instance URL from the POST response headers given by
// |response_info|.
GURL GetApplicationInstanceURL(
    const network::mojom::URLResponseHead& response_info) {
  if (!response_info.headers)
    return GURL();

  // If the application is running after the action specified above, the DIAL
  // server SHALL return an HTTP response with response code 201 Created. In
  // this case, the LOCATION header of the response shall contain an absolute
  // HTTP URL identifying the running instance of the application, known as
  // the Application Instance URL. The host portion of the URL SHALL either
  // resolve to an IPv4 address or be an IPv4 address. No response body shall
  // be returned.
  std::optional<std::string_view> location_header =
      response_info.headers->EnumerateHeader(/*iter=*/nullptr, "LOCATION");
  if (!location_header) {
    return GURL();
  }

  GURL app_instance_url(*location_header);
  if (!app_instance_url.is_valid() || !app_instance_url.SchemeIs("http"))
    return GURL();

  return app_instance_url;
}

}  // namespace

DialLaunchInfo::DialLaunchInfo(const std::string& app_name,
                               const std::optional<std::string>& post_data,
                               const std::string& client_id,
                               const GURL& app_launch_url)
    : app_name(app_name),
      post_data(post_data),
      client_id(client_id),
      app_launch_url(app_launch_url) {}

DialLaunchInfo::DialLaunchInfo(const DialLaunchInfo& other) = default;

DialLaunchInfo::~DialLaunchInfo() = default;

// static
std::unique_ptr<DialActivity> DialActivity::From(
    const std::string& presentation_id,
    const MediaSinkInternal& sink,
    const MediaSource::Id& source_id,
    const url::Origin& client_origin) {
  MediaSource source(source_id);
  GURL url = source.url();
  if (!url.is_valid())
    return nullptr;

  std::string app_name = source.AppNameFromDialSource();
  if (app_name.empty())
    return nullptr;

  std::string client_id;
  std::optional<std::string> post_data;
  // Note: QueryIterator stores the URL by reference, so we must not give it a
  // temporary object.
  for (net::QueryIterator query_it(url); !query_it.IsAtEnd();
       query_it.Advance()) {
    const std::string_view key = query_it.GetKey();
    if (key == "clientId") {
      client_id = std::string(query_it.GetValue());
    } else if (key == "dialPostData") {
      post_data = std::string(query_it.GetValue());
    }
  }
  if (client_id.empty())
    return nullptr;

  GURL app_launch_url = GetAppURL(sink, app_name);
  DCHECK(app_launch_url.is_valid());

  const MediaSink::Id& sink_id = sink.sink().id();
  DialLaunchInfo launch_info(app_name, post_data, client_id, app_launch_url);
  MediaRoute route(
      MediaRoute::GetMediaRouteId(presentation_id, sink_id, source), source,
      sink_id, app_name,
      /* is_local */ true);
  route.set_presentation_id(presentation_id);
  return std::make_unique<DialActivity>(launch_info, route, sink,
                                        client_origin);
}

DialActivity::DialActivity(const DialLaunchInfo& launch_info,
                           const MediaRoute& route,
                           const MediaSinkInternal& sink,
                           const url::Origin& client_origin)
    : launch_info(launch_info),
      route(route),
      sink(sink),
      client_origin(client_origin) {}

DialActivity::~DialActivity() = default;

DialActivity::DialActivity(const DialActivity&) = default;

DialActivityManager::DialActivityManager(
    DialAppDiscoveryService* app_discovery_service)
    : app_discovery_service_(app_discovery_service) {}

DialActivityManager::~DialActivityManager() = default;

void DialActivityManager::AddActivity(const DialActivity& activity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaRoute::Id route_id = activity.route.media_route_id();
  DCHECK(!base::Contains(records_, route_id));
  // TODO(crbug.com/40090609): Consider adding a timeout for transitioning
  // to kLaunched state to clean up unresponsive launches.
  records_.emplace(route_id,
                   std::make_unique<DialActivityManager::Record>(activity));
}

const DialActivity* DialActivityManager::GetActivity(
    const MediaRoute::Id& route_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  return record_it != records_.end() ? &(record_it->second->activity) : nullptr;
}

const DialActivity* DialActivityManager::GetActivityBySinkId(
    const MediaSink::Id& sink_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it =
      base::ranges::find(records_, sink_id, [](const auto& record) {
        return record.second->activity.route.media_sink_id();
      });
  return record_it != records_.end() ? &(record_it->second->activity) : nullptr;
}

const DialActivity* DialActivityManager::GetActivityToJoin(
    const std::string& presentation_id,
    const MediaSource& media_source,
    const url::Origin& client_origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = base::ranges::find_if(
      records_,
      [&presentation_id, &media_source, &client_origin](const auto& record) {
        const auto& route = record.second->activity.route;
        const url::Origin& origin = record.second->activity.client_origin;
        return route.presentation_id() == presentation_id &&
               route.media_source() == media_source && origin == client_origin;
      });
  return record_it != records_.end() ? &(record_it->second->activity) : nullptr;
}

void DialActivityManager::LaunchApp(
    const MediaRoute::Id& route_id,
    const CustomDialLaunchMessageBody& message,
    DialActivityManager::LaunchAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto record_it = records_.find(route_id);
  CHECK(record_it != records_.end());

  auto& record = record_it->second;
  if (record->pending_launch_request ||
      record->state == DialActivityManager::Record::State::kLaunched)
    return;

  if (!message.do_launch) {
    record->state = DialActivityManager::Record::State::kLaunched;
    std::move(callback).Run(true);
    return;
  }

  const DialLaunchInfo& launch_info = record->activity.launch_info;

  // |launch_parameter| overrides original POST data, if it exists.
  const std::optional<std::string>& post_data = message.launch_parameter
                                                    ? message.launch_parameter
                                                    : launch_info.post_data;
  auto fetcher =
      CreateFetcher(base::BindOnce(&DialActivityManager::OnLaunchSuccess,
                                   base::Unretained(this), route_id),
                    base::BindOnce(&DialActivityManager::OnLaunchError,
                                   base::Unretained(this), route_id));
  fetcher->Post(launch_info.app_launch_url, post_data);
  record->pending_launch_request =
      std::make_unique<DialActivityManager::DialLaunchRequest>(
          std::move(fetcher), std::move(callback));
}

std::pair<std::optional<std::string>, mojom::RouteRequestResultCode>
DialActivityManager::CanStopApp(const MediaRoute::Id& route_id) const {
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return {"Activity not found",
            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND};

  if (record_it->second->pending_stop_request) {
    return {"A pending request already exists",
            mojom::RouteRequestResultCode::REDUNDANT_REQUEST};
  }
  return {std::nullopt, mojom::RouteRequestResultCode::OK};
}

void DialActivityManager::StopApp(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  CHECK(record_it != records_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<Record>& record = record_it->second;
  DCHECK(!record->pending_stop_request);

  // Note that it is possible that the app launched on the device, but we
  // haven't received the launch response yet. In this case we will treat it
  // as if it never launched.
  if (record->state != DialActivityManager::Record::State::kLaunched) {
    records_.erase(record_it);
    std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
    return;
  }

  GURL app_instance_url = record->app_instance_url;
  // If |app_instance_url| is not available, try a reasonable fallback.
  if (!app_instance_url.is_valid()) {
    const auto& activity = record->activity;
    app_instance_url =
        GURL(activity.launch_info.app_launch_url.spec() + "/run");
  }

  auto fetcher =
      CreateFetcher(base::BindOnce(&DialActivityManager::OnStopSuccess,
                                   base::Unretained(this), route_id),
                    base::BindOnce(&DialActivityManager::OnStopError,
                                   base::Unretained(this), route_id));
  fetcher->Delete(app_instance_url);
  record->pending_stop_request =
      std::make_unique<DialActivityManager::DialStopRequest>(
          std::move(fetcher), std::move(callback));
}

std::vector<MediaRoute> DialActivityManager::GetRoutes() const {
  std::vector<MediaRoute> routes;
  for (const auto& record : records_)
    routes.push_back(record.second->activity.route);

  return routes;
}

std::unique_ptr<DialURLFetcher> DialActivityManager::CreateFetcher(
    DialURLFetcher::SuccessCallback success_cb,
    DialURLFetcher::ErrorCallback error_cb) {
  // TODO(https://crbug.com/1421142): Add timeout.
  return std::make_unique<DialURLFetcher>(std::move(success_cb),
                                          std::move(error_cb));
}

void DialActivityManager::OnLaunchSuccess(const MediaRoute::Id& route_id,
                                          const std::string& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  auto& record = record_it->second;
  const network::mojom::URLResponseHead* response_info =
      record->pending_launch_request->fetcher->GetResponseHead();

  DCHECK(response_info);
  record->app_instance_url = GetApplicationInstanceURL(*response_info);
  record->state = DialActivityManager::Record::State::kLaunched;
  std::move(record->pending_launch_request->callback).Run(true);
  record->pending_launch_request.reset();
}

void DialActivityManager::OnLaunchError(const MediaRoute::Id& route_id,
                                        const std::string& message,
                                        std::optional<int> response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  // Move the callback out of the record since we are erasing the record.
  auto cb = std::move(record_it->second->pending_launch_request->callback);
  records_.erase(record_it);
  std::move(cb).Run(false);
}

void DialActivityManager::OnStopSuccess(const MediaRoute::Id& route_id,
                                        const std::string& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  // Move the callback out of the record since we are erasing the record.
  auto& record = record_it->second;
  auto cb = std::move(record->pending_stop_request->callback);
  records_.erase(record_it);
  std::move(cb).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
}

void DialActivityManager::OnStopError(const MediaRoute::Id& route_id,
                                      const std::string& message,
                                      std::optional<int> response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  // The vast majority of failures to stop a DIAL session is due to the session
  // no longer existing on the receiver device. So we make another request to
  // the receiver to determine if the session is already terminated.
  app_discovery_service_->FetchDialAppInfo(
      record_it->second->activity.sink,
      record_it->second->activity.launch_info.app_name,
      base::BindOnce(&DialActivityManager::OnInfoFetchedAfterStopError,
                     base::Unretained(this), route_id, message));
}

void DialActivityManager::OnInfoFetchedAfterStopError(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const MediaSink::Id& sink_id,
    const std::string& app_name,
    DialAppInfoResult result) {
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  auto& record = record_it->second;
  auto cb = std::move(record->pending_stop_request->callback);
  records_.erase(record_it);
  if (result.app_info && result.app_info->state != DialAppState::kRunning) {
    // The app is no longer running, so we remove the record and the MediaRoute
    // associated with it.
    std::move(cb).Run(message,
                      mojom::RouteRequestResultCode::ROUTE_ALREADY_TERMINATED);
  } else {
    // The app might still be running, but manually stopping Cast session
    // from DIAL device is not reflected on Chrome side. So, we remove the
    // record and the MediaRoute associated with it here as well.
    // See (crbug.com/1420829) for more context.
    std::move(cb).Run(message, mojom::RouteRequestResultCode::UNKNOWN_ERROR);
  }
}

DialActivityManager::Record::Record(const DialActivity& activity)
    : activity(activity) {}
DialActivityManager::Record::~Record() = default;

}  // namespace media_router
