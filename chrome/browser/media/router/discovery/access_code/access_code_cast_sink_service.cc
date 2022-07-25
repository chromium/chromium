// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media_router {

namespace {

// Connect timeout value when opening a Cast socket.
const int kConnectTimeoutInSeconds = 2;

// Amount of idle time to wait before pinging the Cast device.
const int kPingIntervalInSeconds = 4;

// Amount of idle time to wait before disconnecting.
const int kLivenessTimeoutInSeconds = 8;

using SinkSource = CastDeviceCountMetrics::SinkSource;
using ChannelOpenedCallback = base::OnceCallback<void(bool)>;
constexpr char kLoggerComponent[] = "AccessCodeCastSinkService";

const base::TimeDelta kExpirationDelay = base::Milliseconds(250);

}  // namespace

bool IsAccessCodeCastEnabled() {
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  if (!profile)
    return false;

  return GetAccessCodeCastEnabledPref(profile->GetPrefs());
}

// Callback for adding a remembered sink to the cast list. The second parameter
// is intentionally unused, but it is necessary to match the AddSinkCallback
// type.
void AddRememberedSinkMetricsCallback(AddSinkResultCode result,
                                      absl::optional<std::string> unused) {
  AccessCodeCastMetrics::RecordAddSinkResult(
      true, AddSinkResultMetricsHelper(result));
}

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    AccessCodeMediaRoutesObserver(
        MediaRouter* media_router,
        AccessCodeCastSinkService* access_code_sink_service)
    : MediaRoutesObserver(media_router),
      access_code_sink_service_(access_code_sink_service) {}

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    ~AccessCodeMediaRoutesObserver() = default;

AccessCodeCastSinkService::AccessCodeCastSinkService(
    Profile* profile,
    MediaRouter* media_router,
    CastMediaSinkServiceImpl* cast_media_sink_service_impl,
    DiscoveryNetworkMonitor* network_monitor,
    PrefService* prefs)
    : profile_(profile),
      media_router_(media_router),
      media_routes_observer_(
          std::make_unique<AccessCodeMediaRoutesObserver>(media_router, this)),
      cast_media_sink_service_impl_(cast_media_sink_service_impl),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      network_monitor_(network_monitor),
      prefs_(prefs) {
  DCHECK(profile_) << "The profile does not exist.";
  DCHECK(prefs_)
      << "Prefs could not be fetched from the profile for some reason.";
  backoff_policy_ = {
      // Number of initial errors (in sequence) to ignore before going into
      // exponential backoff.
      0,

      // Initial delay (in ms) once backoff starts.
      1 * 1000,  // 1 second,

      // Factor by which the delay will be multiplied on each subsequent
      // failure. This must be >= 1.0.
      1.0,

      // Fuzzing percentage: 50% will spread delays randomly between 50%--100%
      // of the nominal time.
      0.5,  // 50%

      // Maximum delay (in ms) during exponential backoff.
      1 * 1000,  // 1 second

      // Time to keep an entry from being discarded even when it has no
      // significant state, -1 to never discard.
      1 * 1000,  // 1 second,

      // False means that initial_delay_ms is the first delay once we start
      // exponential backoff, i.e., there is no delay after subsequent
      // successful requests.
      false,
  };
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    // We don't need to post this task per the DiscoveryNetworkMonitor's
    // promise: "All observers will be notified of network changes on the thread
    // from which they registered."
    pref_updater_ = std::make_unique<AccessCodeCastPrefUpdater>(prefs_);
    network_monitor_->AddObserver(this);
    InitAllStoredDevices();
    user_prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
    user_prefs_registrar_->Init(prefs_);
    user_prefs_registrar_->Add(
        prefs::kAccessCodeCastDeviceDuration,
        base::BindRepeating(&AccessCodeCastSinkService::OnDurationPrefChange,
                            base::Unretained(this)));
    user_prefs_registrar_->Add(
        prefs::kAccessCodeCastEnabled,
        base::BindRepeating(&AccessCodeCastSinkService::OnEnabledPrefChange,
                            base::Unretained(this)));
  }
}

AccessCodeCastSinkService::AccessCodeCastSinkService(Profile* profile)
    : AccessCodeCastSinkService(
          profile,
          MediaRouterFactory::GetApiForBrowserContext(profile),
          media_router::DualMediaSinkService::GetInstance()
              ->GetCastMediaSinkServiceImpl(),
          DiscoveryNetworkMonitor::GetInstance(),
          profile->GetPrefs()) {}

AccessCodeCastSinkService::~AccessCodeCastSinkService() = default;

base::WeakPtr<AccessCodeCastSinkService>
AccessCodeCastSinkService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  std::vector<MediaRoute::Id> new_routes;
  for (const auto& route : routes) {
    new_routes.push_back(route.media_route_id());
  }

  std::vector<MediaRoute::Id> removed_routes;
  std::set_difference(old_routes_.begin(), old_routes_.end(),
                      new_routes.begin(), new_routes.end(),
                      std::inserter(removed_routes, removed_routes.end()));
  old_routes_ = new_routes;

  // No routes were removed.
  if (removed_routes.empty())
    return;

  // There should only be 1 element in the |removed_routes| set.
  DCHECK(removed_routes.size() < 2);
  auto first = removed_routes.begin();
  removed_route_id_ = *first;

  base::PostTaskAndReplyWithResult(
      access_code_sink_service_->cast_media_sink_service_impl_->task_runner()
          .get(),
      FROM_HERE,
      base::BindOnce(
          &CastMediaSinkServiceImpl::GetSinkById,
          base::Unretained(
              access_code_sink_service_->cast_media_sink_service_impl_),
          MediaRoute::GetSinkIdFromMediaRouteId(removed_route_id_)),
      base::BindOnce(
          &AccessCodeCastSinkService::HandleMediaRouteDiscoveredByAccessCode,
          access_code_sink_service_->GetWeakPtr()));
}

void AccessCodeCastSinkService::HandleMediaRouteDiscoveredByAccessCode(
    const MediaSinkInternal* sink) {
  // The route Id did not correspond to a sink for some reason. Return to
  // avoid nullptr issues.
  if (!sink)
    return;

  if (!sink->is_cast_sink()) {
    return;
  }

  // Check to see if route was created by an access code sink.
  CastDiscoveryType type = sink->cast_data().discovery_type;
  if (type != CastDiscoveryType::kAccessCodeManualEntry &&
        type != CastDiscoveryType::kAccessCodeRememberedDevice) {
    return;
  }

  media_router_->GetLogger()->LogInfo(
    mojom::LogCategory::kDiscovery, kLoggerComponent,
    "An Access Code Cast route has ended.", sink->id(), "", "");

  // There are two possible cases here. The common case is that a route for
  // the specified sink has been terminated by local or remote user
  // interaction. In this case, call OnAccessCodeRouteRemoved to check whether
  // the sink should now be removed due to expiration. The second case occurs
  // during discovery. It's possible that the discovery process discovered a
  // sink that already existed, and that that sink had an active route. In
  // that case, |OpenChannelIfNecessary| will have terminated that route. It
  // is important though, that we don't attempt to expire the sink in that
  // case, because the user has in fact just "discovered" it. So before
  // attempting to expire the sink, check to see whether the termination was
  // due to discovery. If so, then alert the dialog about the successful
  // discovery.
  auto it = pending_callbacks_.find(sink->id());
  if (it != pending_callbacks_.end()) {
    std::move(it->second).Run(AddSinkResultCode::OK, sink->id());
    pending_callbacks_.erase(sink->id());
  } else {
      // Need to pause just a little bit before attempting to remove the sink.
      // Sometimes sinks terminate their routes and immediately start another
      // (tab content transitions for example), so wait just a little while
      // before checking to see if removing the route makes sense.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeRouteRemoved,
                         weak_ptr_factory_.GetWeakPtr(), sink),
          kExpirationDelay);
  }
}

void AccessCodeCastSinkService::OnAccessCodeRouteRemoved(
    const MediaSinkInternal* sink) {
  // If the sink is a cast sink discovered by Access Code, only expire it if
  // there is a pending expiration. Need to be careful though, because sometimes
  // after a route is removed, another route is immediately reestablished (this
  // can occur if a tab transitions from one type of content (mirroring) to
  // another (preseentation). There was a pause before this method was called,
  // so check again to see if there's an active route for this sink. Only expire
  // the sink if a new route wasn't established during the pause.
  auto route_id = GetActiveRouteId(sink->id());

  // Only remove the sink if there is still no active routes for this sink.
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    // If a sink is pending expiration that means we can
    // remove it from the media router.
    if (!route_id.has_value() && pending_expirations_.count(sink->id())) {
      RemoveSinkIdFromAllEntries(sink->id());
      RemoveMediaSinkFromRouter(sink);
      pending_expirations_.erase(sink->id());
    }
  } else {
    if (!route_id.has_value()) {
      RemoveMediaSinkFromRouter(sink);
    }
  }
}

void AccessCodeCastSinkService::DiscoverSink(const std::string& access_code,
                                             AddSinkResultCallback callback) {
  discovery_server_interface_ =
      std::make_unique<AccessCodeCastDiscoveryInterface>(
          profile_, access_code, media_router_->GetLogger());
  discovery_server_interface_->ValidateDiscoveryAccessCode(
      base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeValidated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AccessCodeCastSinkService::AddSinkToMediaRouter(
    const MediaSinkInternal& sink,
    AddSinkResultCallback add_sink_callback) {
  // Check to see if the media sink already exists in the media router.
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                     base::Unretained(cast_media_sink_service_impl_),
                     sink.id()),
      base::BindOnce(&AccessCodeCastSinkService::OpenChannelIfNecessary,
                     weak_ptr_factory_.GetWeakPtr(), sink,
                     std::move(add_sink_callback)));
}

void AccessCodeCastSinkService::OnAccessCodeValidated(
    AddSinkResultCallback add_sink_callback,
    absl::optional<DiscoveryDevice> discovery_device,
    AddSinkResultCode result_code) {
  if (result_code != AddSinkResultCode::OK) {
    std::move(add_sink_callback).Run(result_code, absl::nullopt);
    return;
  }
  if (!discovery_device.has_value()) {
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::EMPTY_RESPONSE, absl::nullopt);
    return;
  }
  std::pair<absl::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
      creation_result = CreateAccessCodeMediaSink(discovery_device.value());

  if (!creation_result.first.has_value() ||
      creation_result.second != CreateCastMediaSinkResult::kOk) {
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::SINK_CREATION_ERROR, absl::nullopt);
    return;
  }
  auto media_sink = creation_result.first.value();

  AddSinkToMediaRouter(media_sink, std::move(add_sink_callback));
}

void AccessCodeCastSinkService::OpenChannelIfNecessary(
    const MediaSinkInternal& sink,
    AddSinkResultCallback add_sink_callback,
    bool has_sink) {
  if (has_sink) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The sink already exists in the media router, no channel "
        "needs to be opened.",
        sink.id(), "", "");

    // The logic below only pertains to the addition of access code devices that
    // were added via access code (not via stored devices).
    if (sink.cast_data().discovery_type !=
        CastDiscoveryType::kAccessCodeManualEntry) {
      // We must call the |add_sink_callback| in all conditional branches.
      std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink.id());
      return;
    }
    // Check to see if this sink has an active route. If so, we need to
    // terminate the route before alerting the dialog to discovery success.
    // This is because any attempt to start a route on a sink that already has
    // one won't be successful.
    auto route_id = GetActiveRouteId(sink.id());
    if (route_id.has_value()) {
      media_router_->GetLogger()->LogInfo(mojom::LogCategory::kDiscovery,
                                          kLoggerComponent,
                                          "There was an existing route when "
                                          "discovery occurred, attempting to "
                                          "terminate it.",
                                          sink.id(), "", "");
      media_router_->TerminateRoute(route_id.value());
      pending_callbacks_.emplace(sink.id(), std::move(add_sink_callback));
    } else {
      std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink.id());
    }
    return;
  }

  // The OnChannelOpenedResult() callback needs to be be bound with
  // BindPostTask() to ensure that the callback is invoked on this specific task
  // runner.
  auto channel_cb = base::BindOnce(
      &AccessCodeCastSinkService::OnChannelOpenedResult,
      weak_ptr_factory_.GetWeakPtr(), std::move(add_sink_callback), sink.id());

  auto returned_channel_cb =
      base::BindPostTask(task_runner_, std::move(channel_cb));

  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "Attempting to open a cast channel.", sink.id(), "", "");

  switch (sink.cast_data().discovery_type) {
    // For the manual entry case we use our own specific back off and open
    // params so that failure happens much faster.
    case CastDiscoveryType::kAccessCodeManualEntry: {
      auto backoff_entry =
          std::make_unique<net::BackoffEntry>(&backoff_policy_);
      OpenChannelWithParams(std::move(backoff_entry), sink,
                            std::move(returned_channel_cb),
                            CreateCastSocketOpenParams(sink));
      break;
    }
    // For all other cases (such as remembered devices), just use the default
    // parameters that the CastMediaSinkServiceImpl already uses.
    default: {
      base::PostTaskAndReplyWithResult(
          cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::CreateCastSocketOpenParams,
                         base::Unretained(cast_media_sink_service_impl_), sink),
          base::BindOnce(&AccessCodeCastSinkService::OpenChannelWithParams,
                         weak_ptr_factory_.GetWeakPtr(), nullptr, sink,
                         std::move(returned_channel_cb)));
    }
  }
}

void AccessCodeCastSinkService::OpenChannelWithParams(
    std::unique_ptr<net::BackoffEntry> backoff_entry,
    const MediaSinkInternal& sink,
    base::OnceCallback<void(bool)> channel_opened_cb,
    cast_channel::CastSocketOpenParams open_params) {
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel,
                     base::Unretained(cast_media_sink_service_impl_), sink,
                     std::move(backoff_entry), SinkSource::kAccessCode,
                     std::move(channel_opened_cb),
                     CreateCastSocketOpenParams(sink)));
}

absl::optional<const MediaRoute::Id>
AccessCodeCastSinkService::GetActiveRouteId(const MediaSink::Id& sink_id) {
  auto routes = media_router_->GetCurrentRoutes();
  auto route_it = std::find_if(routes.begin(), routes.end(),
                               [&sink_id](const MediaRoute& route) {
                                 return route.media_sink_id() == sink_id;
                               });
  if (route_it == routes.end())
    return absl::nullopt;
  return route_it->media_route_id();
}

cast_channel::CastSocketOpenParams
AccessCodeCastSinkService::CreateCastSocketOpenParams(
    const MediaSinkInternal& sink) {
  return cast_channel::CastSocketOpenParams(
      sink.cast_data().ip_endpoint, base::Seconds(kConnectTimeoutInSeconds),
      base::Seconds(kLivenessTimeoutInSeconds),
      base::Seconds(kPingIntervalInSeconds),
      cast_channel::CastDeviceCapability::NONE);
}

void AccessCodeCastSinkService::OnChannelOpenedResult(
    AddSinkResultCallback add_sink_callback,
    MediaSink::Id sink_id,
    bool channel_opened) {
  if (!channel_opened) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The channel failed to open.", sink_id, "", "");
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, absl::nullopt);
    return;
  }
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "The channel successfully opened.", sink_id, "", "");
  std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink_id);
}

void AccessCodeCastSinkService::StoreSinkAndSetExpirationTimer(
    const MediaSink::Id sink_id) {
  StoreSinkInPrefsById(sink_id);
  SetExpirationTimerById(sink_id);
}

void AccessCodeCastSinkService::StoreSinkInPrefsById(
    const MediaSink::Id sink_id) {
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                     base::Unretained(cast_media_sink_service_impl_), sink_id),
      base::BindOnce(&AccessCodeCastSinkService::StoreSinkInPrefs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::StoreSinkInPrefs(
    const MediaSinkInternal* sink) {
  // For some reason the sink_id isn't in the media router. We can't update
  // prefs.
  if (!sink) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "Unable to remember the cast sink since it was not present in the "
        "media router.",
        "", "", "");
    return;
  }
  pref_updater_->UpdateDevicesDict(*sink);
  pref_updater_->UpdateDeviceAddedTimeDict(sink->id());
}

void AccessCodeCastSinkService::InitAllStoredDevices() {
  auto validated_devices = FetchAndValidateStoredDevices();
  // Record in all instances, even if the number of saved devices is zero.
  AccessCodeCastMetrics::RecordRememberedDevicesCount(validated_devices.size());
  if (validated_devices.empty()) {
    // We don't need anymore logging here since it is already handled in the
    // ValidateStoredDevices function.
    return;
  }
  AddStoredDevicesToMediaRouter(validated_devices);
  InitExpirationTimers(validated_devices);
}

void AccessCodeCastSinkService::InitExpirationTimers(
    const std::vector<MediaSinkInternal> cast_sinks) {
  for (auto cast_sink : cast_sinks) {
    SetExpirationTimer(&cast_sink);
  }
}

void AccessCodeCastSinkService::ResetExpirationTimers() {
  // We must cancel the task of each timer before we clear the map.
  for (auto& timer_pair : current_session_expiration_timers_) {
    timer_pair.second->Stop();
  }
  current_session_expiration_timers_.clear();
}

void AccessCodeCastSinkService::SetExpirationTimerById(
    const MediaSink::Id sink_id) {
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                     base::Unretained(cast_media_sink_service_impl_), sink_id),
      base::BindOnce(&AccessCodeCastSinkService::SetExpirationTimer,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::SetExpirationTimer(
    const MediaSinkInternal* sink) {
  // For some reason the sink_id isn't in the media router. We can't start an
  // expiration timer.
  if (!sink) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "Unable to start an expiration timer for the cast sink since it was "
        "not present in the media router.",
        "", "", "");
    return;
  }

  // Either retrieve collection or create it if it doesn't exist before an
  // operation can occur.
  auto existing_timer = current_session_expiration_timers_.find(sink->id());
  if (existing_timer != current_session_expiration_timers_.end()) {
    // We must first stop the timer before resetting it.
    existing_timer->second->Stop();
  }
  auto expiration_timer = std::make_unique<base::OneShotTimer>();

  // Make sure we include a delay in the case of instant expiration to ensure
  // the sink is not removed before the route is created.
  expiration_timer->Start(
      FROM_HERE,
      CalculateDurationTillExpiration(sink->id()) +
          AccessCodeCastSinkService::kExpirationTimerDelay,
      base::BindOnce(&AccessCodeCastSinkService::OnExpiration,
                     weak_ptr_factory_.GetWeakPtr(), *sink));

  current_session_expiration_timers_[sink->id()] = std::move(expiration_timer);
}

base::TimeDelta AccessCodeCastSinkService::CalculateDurationTillExpiration(
    const MediaSink::Id& sink_id) {
  absl::optional<base::Time> fetched_device_added_time =
      pref_updater_->GetDeviceAddedTime(sink_id);

  if (!fetched_device_added_time.has_value()) {
    media_router_->GetLogger()->LogWarning(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "We couldn't fetch the stored duration for some reason, default to "
        "instantly expiring this sink: " +
            sink_id,
        "", "", "");
    RemoveSinkIdFromAllEntries(sink_id);
    return base::Seconds(0);
  }

  base::Time time_of_expiration = fetched_device_added_time.value() +
                                  GetAccessCodeDeviceDurationPref(prefs_);
  base::TimeDelta time_till_expiration = time_of_expiration - base::Time::Now();

  // If for some reason this value is negative, simply return instant
  // expiration.
  if (time_till_expiration.is_negative())
    return base::Seconds(0);

  return time_till_expiration;
}

const base::Value::List AccessCodeCastSinkService::FetchStoredDevices() {
  return pref_updater_->GetSinkIdsFromDevicesDict();
}

const std::vector<MediaSinkInternal>
AccessCodeCastSinkService::ValidateStoredDevices(
    const base::Value::List& sink_ids) {
  std::vector<MediaSinkInternal> cast_sinks;
  for (const auto& sink_id : sink_ids) {
    const std::string* sink_id_string = sink_id.GetIfString();
    DCHECK(sink_id_string)
        << "The Media Sink id is not stored as a string in the prefs: " +
               sink_ids.DebugString() +
               ". This means something went wrong when storing cast devices "
               "on.";
    auto validation_result = ValidateDeviceFromSinkId(*sink_id_string);

    // Ensure that stored media sink_id corresponds to a properly stored
    // MediaSinkInternal before adding the given sink_id to the media router.
    if (!validation_result.has_value()) {
      media_router_->GetLogger()->LogWarning(
          mojom::LogCategory::kDiscovery, kLoggerComponent,
          "The Media Sink id " + *sink_id_string +
              " is missing from one or more of the pref "
              "services. Attempting to remove all sink_id references right "
              "now.",
          "", "", "");
      RemoveSinkIdFromAllEntries(*sink_id_string);
      continue;
    }
    cast_sinks.push_back(validation_result.value());
  }
  return cast_sinks;
}

const std::vector<MediaSinkInternal>
AccessCodeCastSinkService::FetchAndValidateStoredDevices() {
  auto sink_ids = FetchStoredDevices();
  if (sink_ids.empty()) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "There are no saved Access Code Cast devices for this profile.", "", "",
        "");
    return {};
  }
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "Found Access Code Cast devices for this profile: " +
          sink_ids.DebugString() +
          ". Attempting to validate and then add these cast devices.",
      "", "", "");
  return ValidateStoredDevices(sink_ids);
}

void AccessCodeCastSinkService::AddStoredDevicesToMediaRouter(
    const std::vector<MediaSinkInternal> cast_sinks) {
  std::vector<MediaSinkInternal> cast_sinks_to_add;
  for (auto cast_sink : cast_sinks) {
    AddSinkResultCallback callback =
        base::BindOnce(AddRememberedSinkMetricsCallback);
    AddSinkToMediaRouter(cast_sink, std::move(callback));
  }
}

void AccessCodeCastSinkService::OnExpiration(const MediaSinkInternal& sink) {
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "The sink id: " + sink.id() +
          " has expired. Checking to see if there is an active route, "
          "otherwise remove it from the media router and erase all stored "
          "references.",
      sink.id(), "", "");

  auto route_id = GetActiveRouteId(sink.id());
  // The given sink still has an active route, don't remove it yet and wait for
  // the route to end before we expire it.
  if (route_id.has_value()) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The sink id: " + sink.id() +
            " still has a route open. Wait to expire it until the route has "
            "ended.",
        sink.id(), "", "");
    pending_expirations_.insert(sink.id());
    return;
  }
  RemoveSinkIdFromAllEntries(sink.id());
  // Must find the sink from media router for removal since it has more total
  // information.
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                     base::Unretained(cast_media_sink_service_impl_),
                     sink.id()),
      base::BindOnce(&AccessCodeCastSinkService::RemoveMediaSinkFromRouter,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::RemoveMediaSinkFromRouter(
    const MediaSinkInternal* sink) {
  if (!sink) {
    return;
  }
  DCHECK(!GetActiveRouteId(sink->id()).has_value())
      << "This sink " + sink->id() +
             " still has an active route, we should not be removing it!";
  if (GetActiveRouteId(sink->id()).has_value())
    return;
  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "Attempting to disconnect and remove the cast sink from "
      "the media router.",
      sink->id(), "", "");

  cast_media_sink_service_impl_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::DisconnectAndRemoveSink,
                     base::Unretained(cast_media_sink_service_impl_), *sink),
      kExpirationDelay);
}

void AccessCodeCastSinkService::RemoveSinkIdFromAllEntries(
    const MediaSink::Id& sink_id) {
  pref_updater_->RemoveSinkIdFromDevicesDict(sink_id);
  pref_updater_->RemoveSinkIdFromDeviceAddedTimeDict(sink_id);
}

absl::optional<const MediaSinkInternal>
AccessCodeCastSinkService::ValidateDeviceFromSinkId(
    const MediaSink::Id& sink_id) {
  const auto* sink_value =
      pref_updater_->GetMediaSinkInternalValueBySinkId(sink_id);
  if (!sink_value) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The Media Sink id: " + sink_id +
            " is either stored improperly or doesn't exist within the pref "
            "service.",
        "", "", "");
    return absl::nullopt;
  }
  const auto* dict_value = sink_value->GetIfDict();
  if (!dict_value) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The Media Sink id: " + sink_id +
            " was not stored as a dictionary value in the pref service. Its "
            "storage type is: " +
            base::Value::GetTypeName(sink_value->type()),
        "", "", "");
    return absl::nullopt;
  }
  const absl::optional<MediaSinkInternal> media_sink =
      ParseValueDictIntoMediaSinkInternal(*dict_value);
  if (!media_sink.has_value()) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The Media Sink " + dict_value->DebugString() +
            " could not be parsed from the pref service.",
        "", "", "");
    return absl::nullopt;
  }

  return media_sink.value();
}

void AccessCodeCastSinkService::RemoveExistingSinksOnNetwork() {
  for (auto& sink_id_keypair : current_session_expiration_timers_) {
    auto sink_id = sink_id_keypair.first;
    // If there is an active route for this sink -- don't attempt to remove it.
    // In this case we let the Media Router handle removals from the media
    // router when a network is changed with an active route.
    if (GetActiveRouteId(sink_id).has_value()) {
      continue;
    }

    // There are no active routes for this sink so it is safe to remove from the
    // media router. Must find the sink from media router for removal since it
    // has more total information.
    base::PostTaskAndReplyWithResult(
        cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
        base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                       base::Unretained(cast_media_sink_service_impl_),
                       sink_id),
        base::BindOnce(&AccessCodeCastSinkService::RemoveMediaSinkFromRouter,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void AccessCodeCastSinkService::OnNetworksChanged(
    const std::string& network_id) {
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    RemoveExistingSinksOnNetwork();
    ResetExpirationTimers();
    InitAllStoredDevices();
  }
}

void AccessCodeCastSinkService::OnDurationPrefChange() {
  ResetExpirationTimers();
  InitExpirationTimers(FetchAndValidateStoredDevices());
}

void AccessCodeCastSinkService::OnEnabledPrefChange() {
  if (!GetAccessCodeCastEnabledPref(prefs_)) {
    RemoveExistingSinksOnNetwork();
    ResetExpirationTimers();
    pending_expirations_.clear();
    pref_updater_->ClearDevicesDict();
    pref_updater_->ClearDeviceAddedTimeDict();
  }
}

void AccessCodeCastSinkService::Shutdown() {
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    network_monitor_->RemoveObserver(this);
  }
  // There's no guarantee that MediaRouter is still in the
  // MediaRoutesObserver. |media_routes_observer_| accesses MediaRouter in its
  // dtor. Since MediaRouter and |this| are both KeyedServices, we must not
  // access MediaRouter in the dtor of |this|, so we do it here.
  media_routes_observer_.reset();
  if (user_prefs_registrar_)
    user_prefs_registrar_->RemoveAll();
  user_prefs_registrar_.reset();
  ResetExpirationTimers();
}

}  // namespace media_router
