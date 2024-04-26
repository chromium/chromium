// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_ACTIVITY_MANAGER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media_router {

struct CustomDialLaunchMessageBody;

// Represents parameters used to complete a custom DIAL launch sequence.
struct DialLaunchInfo {
  DialLaunchInfo(const std::string& app_name,
                 const std::optional<std::string>& post_data,
                 const std::string& client_id,
                 const GURL& app_launch_url);
  DialLaunchInfo(const DialLaunchInfo& other);
  ~DialLaunchInfo();

  std::string app_name;

  // The data to include with the app launch POST request. Note this may be
  // overridden by the launchParameter in the CUSTOM_DIAL_LAUNCH response.
  std::optional<std::string> post_data;

  // Cast SDK client ID.
  std::string client_id;

  // The URL to use to launch the app.
  GURL app_launch_url;
};

// Represents custom DIAL launch activity that is to be or has been initiated
// on a DIAL MediaSink.
struct DialActivity {
 public:
  // Returns a DialActivity object created from the given parameters. Returns
  // nullptr if |source_id| does not represent a valid DIAL MediaSource.
  static std::unique_ptr<DialActivity> From(const std::string& presentation_id,
                                            const MediaSinkInternal& sink,
                                            const MediaSource::Id& source_id,
                                            const url::Origin& client_origin);

  DialActivity(const DialLaunchInfo& launch_info,
               const MediaRoute& route,
               const MediaSinkInternal& sink,
               const url::Origin& client_origin);
  ~DialActivity();
  DialActivity(const DialActivity&);

  DialLaunchInfo launch_info;
  MediaRoute route;
  MediaSinkInternal sink;
  // The origin of the web frame that launched the activity.
  url::Origin client_origin;
};

template <typename CallbackType>
struct DialPendingRequest {
 public:
  DialPendingRequest(std::unique_ptr<DialURLFetcher> fetcher,
                     CallbackType callback)
      : fetcher(std::move(fetcher)), callback(std::move(callback)) {}

  DialPendingRequest(const DialPendingRequest&) = delete;
  DialPendingRequest& operator=(const DialPendingRequest&) = delete;

  ~DialPendingRequest() = default;

  std::unique_ptr<DialURLFetcher> fetcher;
  CallbackType callback;
};

// Keeps track of all ongoing custom DIAL launch activities, and is the source
// of truth for DialActivity and MediaRoutes for the DIAL MRP.
// Custom DIAL launch consists of a sequence of message excahnges between the
// MRP and the Cast SDK client:
// (1) When the user requests a custom DIAL launch, |AddActivity()| is called
// with a DialActivity pertaining to the launch. This creates a MediaRoute and
// a connection between the MRP and the SDK client to begin message exchanges.
// (2) The SDK client sends back a custom DIAL launch response. |LaunchApp()|
// is called with the response message which contains additional launch
// parameters.
// (3) After the app is launched, |StopApp()| may be called to terminate the
// app on the receiver.
// All methods on this class must run on the same sequence.
// TODO(crbug.com/40090609): We should be able to simplify the interaction
// between DialMediaRouteProvider and this class once PresentationConnection is
// available to this class to communicate with the page directly.
class DialActivityManager {
 public:
  using LaunchAppCallback = base::OnceCallback<void(bool)>;

  explicit DialActivityManager(DialAppDiscoveryService* app_discovery_service);

  DialActivityManager(const DialActivityManager&) = delete;
  DialActivityManager& operator=(const DialActivityManager&) = delete;

  virtual ~DialActivityManager();

  // Adds |activity| to the manager. This call is valid only if there is no
  // existing activity with the same MediaRoute ID.
  void AddActivity(const DialActivity& activity);

  // Returns the DialActivity associated with |route_id| or nullptr if not
  // found.
  const DialActivity* GetActivity(const MediaRoute::Id& route_id) const;

  // Returns the DialActivity associated with |sink_id| or nullptr if not
  // found.
  const DialActivity* GetActivityBySinkId(const MediaSink::Id& sink_id) const;

  // Returns a DialActivity that can be joined by a request with the given
  // properties, or a nullptr if there is no such activity.
  const DialActivity* GetActivityToJoin(const std::string& presentation_id,
                                        const MediaSource& media_source,
                                        const url::Origin& client_origin) const;

  // Launches the app specified in the activity associated with |route_id|.
  // If |message.launch_parameter| is set, then it overrides the post data
  // originally specified in the activity's DialLaunchInfo.
  // |callback| will be invoked with |true| if the launch succeeded, or |false|
  // if the launch failed.
  // If |message.do_launch| is |false|, then app launch will be skipped, and
  // |callback| will be invoked with |true|.
  // This method is only valid to call if there is an activity for |route_id|.
  // This method is a no-op if there is already a pending launch request, or
  // if the app is already launched.
  void LaunchApp(const MediaRoute::Id& route_id,
                 const CustomDialLaunchMessageBody& message,
                 LaunchAppCallback callback);

  // Checks if there are existing conditions that would cause a stop app request
  // to fail, such as |route_id| being invalid or there already being a pending
  // stop request. If so, returns the error message and error code. Returns
  // nullopt and mojom::RouteRequestResultCode::OK otherwise.
  std::pair<std::optional<std::string>, mojom::RouteRequestResultCode>
  CanStopApp(const MediaRoute::Id& route_id) const;

  // Stops the app that is currently active on |route_id|. Assumes that
  // |route_id| has already been verified with CanStopApp(). On success, the
  // associated DialActivity and MediaRoute will be removed before |callback| is
  // invoked. On failure, the DialActivity and MediaRoute will not be removed.
  void StopApp(const MediaRoute::Id& route_id,
               mojom::MediaRouteProvider::TerminateRouteCallback callback);

  std::vector<MediaRoute> GetRoutes() const;

 private:
  using DialLaunchRequest =
      DialPendingRequest<DialActivityManager::LaunchAppCallback>;
  using DialStopRequest =
      DialPendingRequest<mojom::MediaRouteProvider::TerminateRouteCallback>;

  // Represents the state of a launch activity.
  struct Record {
    explicit Record(const DialActivity& activity);

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    ~Record();

    enum State { kLaunching, kLaunched };

    const DialActivity activity;
    GURL app_instance_url;
    std::unique_ptr<DialLaunchRequest> pending_launch_request;
    std::unique_ptr<DialStopRequest> pending_stop_request;
    State state = kLaunching;
  };

  // Marked virtual for tests.
  virtual std::unique_ptr<DialURLFetcher> CreateFetcher(
      DialURLFetcher::SuccessCallback success_cb,
      DialURLFetcher::ErrorCallback error_cb);

  void OnLaunchSuccess(const MediaRoute::Id& route_id,
                       const std::string& response);
  void OnLaunchError(const MediaRoute::Id& route_id,
                     const std::string& message,
                     std::optional<int> http_response_code);
  void OnStopSuccess(const MediaRoute::Id& route_id,
                     const std::string& response);
  void OnStopError(const MediaRoute::Id& route_id,
                   const std::string& message,
                   std::optional<int> http_response_code);

  void OnInfoFetchedAfterStopError(const MediaRoute::Id& route_id,
                                   const std::string& message,
                                   const MediaSink::Id& sink_id,
                                   const std::string& app_name,
                                   DialAppInfoResult result);

  base::flat_map<MediaRoute::Id, std::unique_ptr<Record>> records_;

  const raw_ptr<DialAppDiscoveryService> app_discovery_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_ACTIVITY_MANAGER_H_
