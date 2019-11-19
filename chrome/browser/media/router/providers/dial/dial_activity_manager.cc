// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"
#include "chrome/common/media_router/media_source.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_response.h"

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
  std::string location_header;
  if (!response_info.headers->EnumerateHeader(nullptr, "LOCATION",
                                              &location_header)) {
    DVLOG(2) << "Missing LOCATION header";
    return GURL();
  }

  GURL app_instance_url(location_header);
  if (!app_instance_url.is_valid() || !app_instance_url.SchemeIs("http"))
    return GURL();

  return app_instance_url;
}

}  // namespace

DialLaunchInfo::DialLaunchInfo(const std::string& app_name,
                               const base::Optional<std::string>& post_data,
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
    bool incognito) {
  MediaSource source(source_id);
  GURL url = source.url();
  if (!url.is_valid())
    return nullptr;

  std::string app_name = source.AppNameFromDialSource();
  if (app_name.empty())
    return nullptr;

  std::string client_id;
  base::Optional<std::string> post_data;
  // Note: QueryIterator stores the URL by reference, so we must not give it a
  // temporary object.
  for (net::QueryIterator query_it(url); !query_it.IsAtEnd();
       query_it.Advance()) {
    std::string key = query_it.GetKey();
    if (key == "clientId") {
      client_id = query_it.GetValue();
    } else if (key == "dialPostData") {
      post_data = query_it.GetValue();
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
      /* is_local */ true, /* for_display */ true);
  route.set_presentation_id(presentation_id);
  route.set_incognito(incognito);
  return std::make_unique<DialActivity>(launch_info, route);
}

DialActivity::DialActivity(const DialLaunchInfo& launch_info,
                           const MediaRoute& route)
    : launch_info(launch_info), route(route) {}

DialActivity::~DialActivity() = default;

DialActivityManager::DialActivityManager() = default;

DialActivityManager::~DialActivityManager() = default;

void DialActivityManager::AddActivity(const DialActivity& activity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaRoute::Id route_id = activity.route.media_route_id();
  DCHECK(!base::Contains(records_, route_id));
  // TODO(https://crbug.com/816628): Consider adding a timeout for transitioning
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
  auto record_it = std::find_if(
      records_.begin(), records_.end(), [&sink_id](const auto& record) {
        return record.second->activity.route.media_sink_id() == sink_id;
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
    DVLOG(2) << "Launch will be handled by SDK client; skipping launch.";
    record->state = DialActivityManager::Record::State::kLaunched;
    std::move(callback).Run(true);
    return;
  }

  const DialLaunchInfo& launch_info = record->activity.launch_info;

  // |launch_parameter| overrides original POST data, if it exists.
  const base::Optional<std::string>& post_data = message.launch_parameter
                                                     ? message.launch_parameter
                                                     : launch_info.post_data;
  DVLOG(2) << "Launching app on " << route_id;

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

std::pair<base::Optional<std::string>, RouteRequestResult::ResultCode>
DialActivityManager::CanStopApp(const MediaRoute::Id& route_id) const {
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return {"Activity not found", RouteRequestResult::ROUTE_NOT_FOUND};

  if (record_it->second->pending_stop_request) {
    return {"A pending request already exists",
            RouteRequestResult::UNKNOWN_ERROR};
  }
  return {base::nullopt, RouteRequestResult::OK};
}

void DialActivityManager::StopApp(
    const MediaRoute::Id& route_id,
    mojom::MediaRouteProvider::TerminateRouteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto record_it = records_.find(route_id);
  DCHECK(record_it != records_.end());
  std::unique_ptr<Record>& record = record_it->second;
  DCHECK(!record->pending_stop_request);

  // Note that it is possible that the app launched on the device, but we
  // haven't received the launch response yet. In this case we will treat it
  // as if it never launched.
  if (record->state != DialActivityManager::Record::State::kLaunched) {
    DVLOG(2) << "App didn't launch; not issuing DELETE request.";
    records_.erase(record_it);
    std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
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
  // TODO(https://crbug.com/816628): Add timeout.
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
                                        int response_code,
                                        const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(2) << "Response code: " << response_code << ", message: " << message;
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
  std::move(cb).Run(base::nullopt, RouteRequestResult::OK);
}

void DialActivityManager::OnStopError(const MediaRoute::Id& route_id,
                                      int response_code,
                                      const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(2) << "Response code: " << response_code << ", message: " << message;
  auto record_it = records_.find(route_id);
  if (record_it == records_.end())
    return;

  // Move the callback out of the record since we are erasing the record.
  auto& record = record_it->second;
  auto cb = std::move(record->pending_stop_request->callback);
  record->pending_stop_request.reset();
  std::move(cb).Run(message, RouteRequestResult::UNKNOWN_ERROR);
}

DialActivityManager::Record::Record(const DialActivity& activity)
    : activity(activity) {}
DialActivityManager::Record::~Record() = default;

}  // namespace media_router
