// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
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

}  // namespace

bool IsAccessCodeCastEnabled() {
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  if (!profile)
    return false;

  return GetAccessCodeCastEnabledPref(profile);
}

// Callback for adding a remembered sink to the cast list. The second parameter
// is intentionally unused, but it is necessary to match the AddSinkCallback
// type.
void AddRememberedSinkMetricsCallback(AddSinkResultCode result,
                                      std::optional<std::string> unused) {
  AccessCodeCastMetrics::RecordAddSinkResult(
      true, AddSinkResultMetricsHelper(result));
}

AccessCodeCastSinkService::AccessCodeCastSinkService(
    Profile* profile,
    MediaRouter* media_router,
    CastMediaSinkServiceImpl* cast_media_sink_service_impl,
    DiscoveryNetworkMonitor* network_monitor,
    PrefService* prefs,
    std::unique_ptr<AccessCodeCastPrefUpdater> pref_updater)
    : profile_(profile),
      media_router_(media_router),
      media_routes_observer_(
          std::make_unique<AccessCodeMediaRoutesObserver>(media_router, this)),
      cast_media_sink_service_impl_(cast_media_sink_service_impl),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      network_monitor_(network_monitor),
      prefs_(prefs),
      pref_updater_(std::move(pref_updater)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  DCHECK(profile_) << "The profile does not exist.";
  DCHECK(prefs_)
      << "Prefs could not be fetched from the profile for some reason.";
  DCHECK(media_router_) << "The media router does not exist.";
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

  InitializePrefUpdater();

  // We don't need to post this task per the DiscoveryNetworkMonitor's
  // promise: "All observers will be notified of network changes on the thread
  // from which they registered."
  network_monitor_->AddObserver(this);
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

AccessCodeCastSinkService::AccessCodeCastSinkService(Profile* profile)
    : AccessCodeCastSinkService(
          profile,
          MediaRouterFactory::GetApiForBrowserContext(profile),
          media_router::DualMediaSinkService::GetInstance()
              ->GetCastMediaSinkServiceImpl(),
          DiscoveryNetworkMonitor::GetInstance(),
          profile->GetPrefs(),
          /* pref_updater */ nullptr) {}

AccessCodeCastSinkService::~AccessCodeCastSinkService() = default;

base::WeakPtr<AccessCodeCastSinkService>
AccessCodeCastSinkService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    AccessCodeMediaRoutesObserver(
        MediaRouter* media_router,
        AccessCodeCastSinkService* access_code_sink_service)
    : MediaRoutesObserver(media_router),
      access_code_sink_service_(access_code_sink_service) {}

AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    ~AccessCodeMediaRoutesObserver() = default;

void AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::
    OnRoutesUpdatedForTesting(const std::vector<MediaRoute>& routes) {
  OnRoutesUpdated(routes);
}

void AccessCodeCastSinkService::AccessCodeMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  std::vector<MediaRoute::Id> new_routes;
  for (const auto& route : routes) {
    new_routes.push_back(route.media_route_id());
  }

  std::vector<MediaRoute::Id> removed_routes;
  std::set_difference(previous_routes_.begin(), previous_routes_.end(),
                      new_routes.begin(), new_routes.end(),
                      std::inserter(removed_routes, removed_routes.end()));

  std::vector<MediaRoute::Id> added_routes;
  std::set_difference(new_routes.begin(), new_routes.end(),
                      previous_routes_.begin(), previous_routes_.end(),
                      std::inserter(added_routes, added_routes.end()));

  previous_routes_ = new_routes;

  if (added_routes.size() > 0) {
    auto new_route_id = *added_routes.begin();

    // Only record the start time for local routes
    bool is_route_local = false;
    MediaSource source = MediaSource::ForAnyTab();
    for (const auto& route : routes) {
      if (route.media_route_id() == new_route_id && route.is_local()) {
        is_route_local = true;
        source = route.media_source();
      }
    }

    access_code_sink_service_->GetCastMediaSinkServiceImpl()
        ->task_runner()
        ->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                &CastMediaSinkServiceImpl::GetSinkById,
                base::Unretained(
                    access_code_sink_service_->GetCastMediaSinkServiceImpl()),
                MediaRoute::GetSinkIdFromMediaRouteId(new_route_id)),
            base::BindOnce(&AccessCodeCastSinkService::HandleMediaRouteAdded,
                           access_code_sink_service_->GetWeakPtr(),
                           new_route_id, is_route_local, source));
  }

  // No routes were removed.
  if (removed_routes.empty())
    return;

  // There should only be 1 element in the |removed_routes| set.
  DCHECK(removed_routes.size() < 2)
      << "This value should only be 1, since only one route can be removed at "
         "a time.";
  auto first = removed_routes.begin();
  removed_route_id_ = *first;

  auto route_start_times =
      access_code_sink_service_->current_route_start_times_;
  auto route_start_time_iterator = route_start_times.find(removed_route_id_);
  if (route_start_time_iterator != route_start_times.end()) {
    base::Time route_start_time = route_start_time_iterator->second;
    AccessCodeCastMetrics::RecordRouteDuration(base::Time::Now() -
                                               route_start_time);
    route_start_times.erase(removed_route_id_);
  }

  access_code_sink_service_->GetCastMediaSinkServiceImpl()
      ->task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &CastMediaSinkServiceImpl::GetSinkById,
              base::Unretained(
                  access_code_sink_service_->GetCastMediaSinkServiceImpl()),
              MediaRoute::GetSinkIdFromMediaRouteId(removed_route_id_)),
          base::BindOnce(
              &AccessCodeCastSinkService::HandleMediaRouteRemovedByAccessCode,
              access_code_sink_service_->GetWeakPtr()));
}

bool AccessCodeCastSinkService::IsSinkValidAccessCodeSink(
    const MediaSinkInternal* sink) {
  // The route Id did not correspond to a sink for some reason. Return to
  // avoid nullptr issues.
  if (!sink || !sink->is_cast_sink())
    return false;

  // Check to see if route was created by an access code sink.
  CastDiscoveryType type = sink->cast_data().discovery_type;
  if (type != CastDiscoveryType::kAccessCodeManualEntry &&
      type != CastDiscoveryType::kAccessCodeRememberedDevice) {
    return false;
  }
  return true;
}

void AccessCodeCastSinkService::HandleMediaRouteRemovedByAccessCode(
    const MediaSinkInternal* sink) {
  if (!IsSinkValidAccessCodeSink(sink)) {
    return;
  }

  LogInfo("An Access Code Cast route has ended.", sink->id());

  // Need to pause just a little bit before attempting to remove the sink.
  // Sometimes sinks terminate their routes and immediately start another
  // (tab content transitions for example), so wait just a little while
  // before checking to see if removing the route makes sense.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeRouteRemoved,
                     GetWeakPtr(), sink),
      kExpirationDelay);
}

void AccessCodeCastSinkService::HandleMediaRouteAdded(
    const MediaRoute::Id route_id,
    const bool is_route_local,
    const MediaSource media_source,
    const MediaSinkInternal* sink) {
  if (!IsSinkValidAccessCodeSink(sink))
    return;

  if (is_route_local) {
    current_route_start_times_[route_id] = base::Time::Now();
  }

  bool is_saved = sink->cast_data().discovery_type ==
                  CastDiscoveryType::kAccessCodeRememberedDevice;
  AccessCodeCastCastMode source_type = AccessCodeCastCastMode::kPresentation;
  if (media_source.IsTabMirroringSource()) {
    source_type = AccessCodeCastCastMode::kTabMirror;
  } else if (media_source.IsDesktopMirroringSource()) {
    source_type = AccessCodeCastCastMode::kDesktopMirror;
  } else if (media_source.IsRemotePlaybackSource()) {
    source_type = AccessCodeCastCastMode::kRemotePlayback;
  }

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      GetAccessCodeDeviceDurationPref(profile_), is_saved, source_type);
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
  auto route = GetActiveRoute(sink->id());

  // If there is no active route, check manually if the device should be
  // instantly expired.
  if (!route.has_value())
    CheckMediaSinkForExpiration(sink->id());
}

void AccessCodeCastSinkService::DiscoverSink(const std::string& access_code,
                                             AddSinkResultCallback callback) {
  if (!media_router_) {
    // We cannot log this error since we cannot get access to the
    // media_router logger. Instead, this will error will be surfaced in
    // AccessCodeCast histograms.
    std::move(callback).Run(AddSinkResultCode::INTERNAL_MEDIA_ROUTER_ERROR,
                            std::nullopt);
    return;
  }
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    LogWarning(
        "We are not either not connected to a valid network or not connected "
        "to any network.",
        "");
    std::move(callback).Run(AddSinkResultCode::SERVICE_NOT_PRESENT,
                            std::nullopt);
    return;
  }
  discovery_server_interface_ =
      std::make_unique<AccessCodeCastDiscoveryInterface>(
          profile_, access_code, media_router_->GetLogger(), identity_manager_);
  discovery_server_interface_->ValidateDiscoveryAccessCode(
      base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeValidated,
                     GetWeakPtr(), std::move(callback)));
}

void AccessCodeCastSinkService::AddSinkToMediaRouter(
    const MediaSinkInternal& sink,
    AddSinkResultCallback add_sink_callback) {
  DCHECK(cast_media_sink_service_impl_)
      << "Must have a valid CastMediaSinkServiceImpl!";

  // Check to see if the media sink already exists in the media router.
  cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                     base::Unretained(cast_media_sink_service_impl_),
                     sink.id()),
      base::BindOnce(&AccessCodeCastSinkService::OpenChannelIfNecessary,
                     GetWeakPtr(), sink, std::move(add_sink_callback)));
}

void AccessCodeCastSinkService::OnAccessCodeValidated(
    AddSinkResultCallback add_sink_callback,
    std::optional<DiscoveryDevice> discovery_device,
    AddSinkResultCode result_code) {
  if (result_code != AddSinkResultCode::OK) {
    std::move(add_sink_callback).Run(result_code, std::nullopt);
    return;
  }
  if (!discovery_device.has_value()) {
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::EMPTY_RESPONSE, std::nullopt);
    return;
  }
  std::pair<std::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
      creation_result = CreateAccessCodeMediaSink(discovery_device.value());

  if (!creation_result.first.has_value() ||
      creation_result.second != CreateCastMediaSinkResult::kOk) {
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::SINK_CREATION_ERROR, std::nullopt);
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
    LogInfo(
        "The sink already exists in the media router, no channel "
        "needs to be opened.",
        sink.id());

    // Only refresh data if this new sink came from typing in an access code.
    if (sink.cast_data().discovery_type ==
        CastDiscoveryType::kAccessCodeManualEntry) {
      // We can't store the sink by ID, since that will pull the outdated
      // information already in the media router.
      // `SetExpirationTimer()` needs to query the `pref_updater_` for the
      // device addition time, so it must be called after
      // `StoreSinkInPrefsById()` has finished.
      StoreSinkInPrefs(
          base::BindOnce(&AccessCodeCastSinkService::SetExpirationTimer,
                         GetWeakPtr(), sink.id()),
          &sink);

      // Get the existing sink so we can update its info.
      cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                         base::Unretained(cast_media_sink_service_impl_),
                         sink.id()),
          base::BindOnce(&AccessCodeCastSinkService::UpdateExistingSink,
                         GetWeakPtr(), sink));
    }

    std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink.id());
    return;
  }

  // The OnChannelOpenedResult() callback needs to be be bound with
  // BindPostTask() to ensure that the callback is invoked on this specific task
  // runner.
  auto channel_cb =
      base::BindOnce(&AccessCodeCastSinkService::OnChannelOpenedResult,
                     GetWeakPtr(), std::move(add_sink_callback), sink);

  auto returned_channel_cb =
      base::BindPostTask(task_runner_, std::move(channel_cb));

  LogInfo("Attempting to open a cast channel.", sink.id());

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
      cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::CreateCastSocketOpenParams,
                         base::Unretained(cast_media_sink_service_impl_), sink),
          base::BindOnce(&AccessCodeCastSinkService::OpenChannelWithParams,
                         GetWeakPtr(), nullptr, sink,
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

std::optional<const MediaRoute> AccessCodeCastSinkService::GetActiveRoute(
    const MediaSink::Id& sink_id) {
  if (!media_router_)
    return std::nullopt;
  auto routes = media_router_->GetCurrentRoutes();
  auto route_it =
      base::ranges::find(routes, sink_id, &MediaRoute::media_sink_id);
  if (route_it == routes.end())
    return std::nullopt;
  return *route_it;
}

cast_channel::CastSocketOpenParams
AccessCodeCastSinkService::CreateCastSocketOpenParams(
    const MediaSinkInternal& sink) {
  return cast_channel::CastSocketOpenParams(
      sink.cast_data().ip_endpoint, base::Seconds(kConnectTimeoutInSeconds),
      base::Seconds(kLivenessTimeoutInSeconds),
      base::Seconds(kPingIntervalInSeconds), /*CastDeviceCapabilitySet*/ {});
}

void AccessCodeCastSinkService::OnChannelOpenedResult(
    AddSinkResultCallback add_sink_callback,
    const MediaSinkInternal& sink,
    bool channel_opened) {
  if (!channel_opened) {
    LogError("The channel failed to open.", sink.id());
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, std::nullopt);
    return;
  }
  LogInfo("The channel successfully opened.", sink.id());
  std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink.id());
  if (sink.cast_data().discovery_type ==
      CastDiscoveryType::kAccessCodeManualEntry) {
    StoreSinkAndSetExpirationTimer(sink.id());
  }
}

void AccessCodeCastSinkService::CheckMediaSinkForExpiration(
    const MediaSink::Id& sink_id) {
  CalculateDurationTillExpiration(
      sink_id,
      base::BindOnce(&AccessCodeCastSinkService::DoCheckMediaSinkForExpiration,
                     GetWeakPtr(), sink_id));
}

void AccessCodeCastSinkService::ShutdownForTesting() {
  Shutdown();
}

void AccessCodeCastSinkService::ResetPrefUpdaterForTesting() {
  pref_updater_.reset();
}

void AccessCodeCastSinkService::StoreSinkInPrefsForTesting(
    base::OnceClosure on_sink_stored_callback,
    const MediaSinkInternal* sink) {
  StoreSinkInPrefs(std::move(on_sink_stored_callback), sink);
}

void AccessCodeCastSinkService::SetExpirationTimerForTesting(
    const MediaSink::Id& sink_id) {
  SetExpirationTimer(sink_id);
}

void AccessCodeCastSinkService::OpenChannelIfNecessaryForTesting(
    const MediaSinkInternal& sink,
    AddSinkResultCallback add_sink_callback,
    bool has_sink) {
  OpenChannelIfNecessary(sink, std::move(add_sink_callback), has_sink);
}

void AccessCodeCastSinkService::HandleMediaRouteAddedForTesting(
    const MediaRoute::Id route_id,
    const bool is_route_local,
    const MediaSource media_source,
    const MediaSinkInternal* sink) {
  HandleMediaRouteAdded(route_id, is_route_local, media_source, sink);
}

void AccessCodeCastSinkService::HandleMediaRouteRemovedByAccessCodeForTesting(
    const MediaSinkInternal* sink) {
  HandleMediaRouteRemovedByAccessCode(sink);
}

void AccessCodeCastSinkService::OnAccessCodeValidatedForTesting(
    AddSinkResultCallback add_sink_callback,
    std::optional<DiscoveryDevice> discovery_device,
    AddSinkResultCode result_code) {
  OnAccessCodeValidated(std::move(add_sink_callback), discovery_device,
                        result_code);
}

void AccessCodeCastSinkService::OnChannelOpenedResultForTesting(
    AddSinkResultCallback add_sink_callback,
    const MediaSinkInternal& sink,
    bool channel_opened) {
  OnChannelOpenedResult(std::move(add_sink_callback), sink, channel_opened);
}

void AccessCodeCastSinkService::InitializePrefUpdaterForTesting() {
  InitializePrefUpdater();
}
void AccessCodeCastSinkService::InitAllStoredDevicesForTesting() {
  InitAllStoredDevices();
}

void AccessCodeCastSinkService::CalculateDurationTillExpirationForTesting(
    const MediaSink::Id& sink_id,
    base::OnceCallback<void(base::TimeDelta)> on_duration_calculated_callback) {
  CalculateDurationTillExpiration(sink_id,
                                  std::move(on_duration_calculated_callback));
}

void AccessCodeCastSinkService::DoCheckMediaSinkForExpiration(
    const MediaSink::Id& sink_id,
    base::TimeDelta time_till_expiration) {
  if (!time_till_expiration.is_zero()) {
    return;
  }

  auto iterator = current_session_expiration_timers_.find(sink_id);

  // Check to see if there exists a timer for the given media sink id.
  if (iterator == current_session_expiration_timers_.end()) {
    LogWarning(
        "While manually checking if the sink has expired, the given media sink "
        "id does not have an active expiration timer.",
        sink_id);
    return;
  }
  auto& expiration_timer = iterator->second;
  if (!expiration_timer->IsRunning()) {
    LogInfo(
        "While manually checking if the sink has expired, we found that the "
        "expiration timer has already fired so there is no need to re-trigger "
        "it.",
        sink_id);
    ExpireSink(sink_id);
    return;
  }

  // Instantly fire the timer and remove it from the map.
  expiration_timer->FireNow();
  current_session_expiration_timers_.erase(iterator);
}

void AccessCodeCastSinkService::OnObserverRoutesUpdatedForTesting(
    const std::vector<MediaRoute>& routes) {
  media_routes_observer_->OnRoutesUpdatedForTesting(routes);  // IN-TEST
}

void AccessCodeCastSinkService::InitAllStoredDevices() {
  FetchAndValidateStoredDevices(
      base::BindOnce(&AccessCodeCastSinkService::OnStoredDevicesValidated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::OnStoredDevicesValidated(
    const std::vector<MediaSinkInternal>& validated_devices) {
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

void AccessCodeCastSinkService::FetchAndValidateStoredDevices(
    base::OnceCallback<void(const std::vector<MediaSinkInternal>&)>
        on_device_validated_callback) {
  if (!pref_updater_) {
    LogError(
        "Failed to fetch stored devices: pref_updater_ hasn't been "
        "instantiated.",
        "");
    std::move(on_device_validated_callback).Run({});
    return;
  }
  pref_updater_->GetDevicesDict(base::BindOnce(
      &AccessCodeCastSinkService::ValidateStoredDevices,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_device_validated_callback)));
}

void AccessCodeCastSinkService::ValidateStoredDevices(
    base::OnceCallback<void(const std::vector<MediaSinkInternal>&)>
        on_device_validated_callback,
    base::Value::Dict stored_sinks) {
  if (stored_sinks.empty()) {
    LogInfo("There are no saved Access Code Cast devices for this profile.",
            "");
    std::move(on_device_validated_callback).Run({});
    return;
  }

  std::vector<MediaSinkInternal> validated_sinks;
  std::vector<MediaSink::Id> invalid_sinks;
  for (const auto sink_value : stored_sinks) {
    const std::string& sink_id_string = sink_value.first;
    const auto* dict_value = sink_value.second.GetIfDict();
    if (!dict_value) {
      LogError(
          "The Media Sink id: " + sink_id_string +
              " was not stored as a dictionary value in the pref service. Its "
              "storage type is: " +
              base::Value::GetTypeName(sink_value.second.type()),
          "");
      invalid_sinks.push_back(sink_id_string);
      continue;
    }

    const std::optional<MediaSinkInternal> media_sink =
        ParseValueDictIntoMediaSinkInternal(*dict_value);
    if (!media_sink.has_value()) {
      LogWarning(
          "The media sink is missing from one or more of the pref "
          "services. Attempting to remove all sink_id references right "
          "now.",
          sink_id_string);
      invalid_sinks.push_back(sink_id_string);
      continue;
    }
    validated_sinks.push_back(media_sink.value());
  }

  for (const auto& sink_id : invalid_sinks) {
    RemoveSinkIdFromAllEntries(sink_id);
  }

  std::move(on_device_validated_callback).Run(validated_sinks);
}

void AccessCodeCastSinkService::InitExpirationTimers(
    const std::vector<MediaSinkInternal>& cast_sinks) {
  for (auto cast_sink : cast_sinks) {
    SetExpirationTimer(cast_sink.id());
  }
}

void AccessCodeCastSinkService::SetExpirationTimer(
    const MediaSink::Id& sink_id) {
  CalculateDurationTillExpiration(
      sink_id, base::BindOnce(&AccessCodeCastSinkService::DoSetExpirationTimer,
                              GetWeakPtr(), sink_id));
}

void AccessCodeCastSinkService::DoSetExpirationTimer(
    const MediaSink::Id& sink_id,
    base::TimeDelta time_till_expiration) {
  // Either retrieve collection or create it if it doesn't exist before an
  // operation can occur.
  auto existing_timer = current_session_expiration_timers_.find(sink_id);
  if (existing_timer != current_session_expiration_timers_.end()) {
    // We must first stop the timer before resetting it.
    existing_timer->second->Stop();
  }

  auto expiration_timer = std::make_unique<base::OneShotTimer>();
  // Make sure we include a delay in the case of instant expiration to ensure
  // the sink is not removed before the route is created.
  expiration_timer->Start(
      FROM_HERE,
      time_till_expiration + AccessCodeCastSinkService::kExpirationTimerDelay,
      base::BindOnce(&AccessCodeCastSinkService::OnExpiration, GetWeakPtr(),
                     sink_id));

  current_session_expiration_timers_[sink_id] = std::move(expiration_timer);
}

void AccessCodeCastSinkService::ResetExpirationTimers() {
  // We must cancel the task of each timer before we clear the map.
  for (auto& timer_pair : current_session_expiration_timers_) {
    timer_pair.second->Stop();
  }
  current_session_expiration_timers_.clear();
}

void AccessCodeCastSinkService::CalculateDurationTillExpiration(
    const MediaSink::Id& sink_id,
    base::OnceCallback<void(base::TimeDelta)> on_duration_calculated_callback) {
  if (!pref_updater_) {
    LogError(
        "Failed to calculate duration till expiration: pref_updater_ hasn't "
        "been instantiated.",
        sink_id);
    std::move(on_duration_calculated_callback).Run(base::Seconds(0));
    return;
  }
  pref_updater_->GetDeviceAddedTime(
      sink_id,
      base::BindOnce(
          &AccessCodeCastSinkService::DoCalculateDurationTillExpiration,
          GetWeakPtr(), sink_id, std::move(on_duration_calculated_callback)));
}

void AccessCodeCastSinkService::DoCalculateDurationTillExpiration(
    const MediaSink::Id& sink_id,
    base::OnceCallback<void(base::TimeDelta)> on_duration_calculated_callback,
    std::optional<base::Time> fetched_device_added_time) {
  if (!fetched_device_added_time.has_value()) {
    LogWarning(
        "We couldn't fetch the stored duration for some reason, default to "
        "instantly expiring this sink: " +
            sink_id,
        "");
    std::move(on_duration_calculated_callback).Run(base::Seconds(0));
    return;
  }

  base::Time time_of_expiration = fetched_device_added_time.value() +
                                  GetAccessCodeDeviceDurationPref(profile_);
  base::TimeDelta time_till_expiration = time_of_expiration - base::Time::Now();

  // If for some reason this value is negative, simply return instant
  // expiration.
  if (time_till_expiration.is_negative()) {
    std::move(on_duration_calculated_callback).Run(base::Seconds(0));
  } else {
    std::move(on_duration_calculated_callback).Run(time_till_expiration);
  }
}

void AccessCodeCastSinkService::OnExpiration(const MediaSink::Id& sink_id) {
  LogInfo("The sink id: " + sink_id +
              " has expired. Checking to see if there is an active route, "
              "otherwise remove it from the media router and erase all stored "
              "references.",
          sink_id);

  auto route = GetActiveRoute(sink_id);
  // The given sink still has an active route, don't remove it yet and wait for
  // the route to end before we expire it.
  if (route.has_value() && route.value().is_local()) {
    LogInfo("The sink id: " + sink_id +
                " still has a local route open. Wait to expire it until the "
                "route has ended.",
            sink_id);
    return;
  }

  ExpireSink(sink_id);
}

void AccessCodeCastSinkService::ExpireSink(const MediaSink::Id& sink_id) {
  RemoveSinkIdFromAllEntries(sink_id);

  // Must find the sink from media router for removal since it has more total
  // information.
  cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                     base::Unretained(cast_media_sink_service_impl_), sink_id),
      base::BindOnce(
          &AccessCodeCastSinkService::RemoveAndDisconnectMediaSinkFromRouter,
          GetWeakPtr()));
}

void AccessCodeCastSinkService::StoreSinkInPrefsById(
    const MediaSink::Id& sink_id,
    base::OnceClosure on_sink_stored_callback) {
  cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                     base::Unretained(cast_media_sink_service_impl_), sink_id),
      base::BindOnce(&AccessCodeCastSinkService::StoreSinkInPrefs, GetWeakPtr(),
                     std::move(on_sink_stored_callback)));
}

void AccessCodeCastSinkService::StoreSinkInPrefs(
    base::OnceClosure on_sink_stored_callback,
    const MediaSinkInternal* sink) {
  // For some reason the sink_id isn't in the media router. We can't update
  // prefs.
  if (!sink) {
    LogError(
        "Unable to remember the cast sink since it was not present in the "
        "media router.",
        "");
    return;
  }
  if (!pref_updater_) {
    LogError(
        "Failed to store the sink in prefs: pref_updater_ hasn't been "
        "instantiated.",
        sink->id());
    return;
  }
  // Enforce the ordering of updating the pref service so that when ChromeOS
  // gets notified of changes in the devices dict, it's guaranteed that the
  // device added time dict has been updated and can be used to set proper
  // expiration timers.
  pref_updater_->UpdateDeviceAddedTimeDict(
      sink->id(),
      base::BindOnce(
          [](AccessCodeCastPrefUpdater* pref_updater,
             const MediaSinkInternal* sink, base::OnceClosure callback) {
            pref_updater->UpdateDevicesDict(*sink, std::move(callback));
          },
          pref_updater_.get(), sink, std::move(on_sink_stored_callback)));
}

void AccessCodeCastSinkService::StoreSinkAndSetExpirationTimer(
    const MediaSink::Id& sink_id) {
  // `SetExpirationTimer` needs to query the `pref_updater_` for the device
  // addition time, so it must be called after `StoreSinkInPrefsById()` has
  // finished.
  StoreSinkInPrefsById(
      sink_id, base::BindOnce(&AccessCodeCastSinkService::SetExpirationTimer,
                              GetWeakPtr(), sink_id));
}

void AccessCodeCastSinkService::AddStoredDevicesToMediaRouter(
    const std::vector<MediaSinkInternal>& cast_sinks) {
  std::vector<MediaSinkInternal> cast_sinks_to_add;
  for (auto cast_sink : cast_sinks) {
    AddSinkResultCallback callback =
        base::BindOnce(AddRememberedSinkMetricsCallback);
    AddSinkToMediaRouter(cast_sink, std::move(callback));
  }
}

void AccessCodeCastSinkService::UpdateExistingSink(
    const MediaSinkInternal& new_sink,
    const MediaSinkInternal* existing_sink) {
  // AddOrUpdate takes time, so avoid calling it if we can
  if (existing_sink->sink().name() == new_sink.sink().name() &&
      existing_sink->cast_data().capabilities ==
          new_sink.cast_data().capabilities)
    return;

  MediaSinkInternal existing_sink_copy = *existing_sink;
  existing_sink_copy.sink().set_name(new_sink.sink().name());
  auto capabilities = new_sink.cast_data().capabilities;
  existing_sink_copy.cast_data().capabilities = capabilities;
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::AddOrUpdateSink,
                                base::Unretained(cast_media_sink_service_impl_),
                                existing_sink_copy));
}

void AccessCodeCastSinkService::RemoveSinkIdFromAllEntries(
    const MediaSink::Id& sink_id) {
  if (!pref_updater_) {
    LogError(
        "Failed to remove the sink from prefs: pref_updater_ hasn't been "
        "instantiated.",
        sink_id);
    return;
  }
  pref_updater_->RemoveSinkIdFromDevicesDict(sink_id, base::DoNothing());
  pref_updater_->RemoveSinkIdFromDeviceAddedTimeDict(sink_id,
                                                     base::DoNothing());
}

void AccessCodeCastSinkService::RemoveAndDisconnectMediaSinkFromRouter(
    const MediaSinkInternal* sink) {
  if (!sink) {
    return;
  }

  // We don't want to remove a media sink that has an active route that is ALSO
  // a local route (casting the contents of this client).
  if (GetActiveRoute(sink->id()).has_value() &&
      GetActiveRoute(sink->id()).value().is_local()) {
    return;
  }
  LogInfo(
      "Attempting to disconnect and remove the cast sink from "
      "the media router.",
      sink->id());
  cast_media_sink_service_impl_->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::DisconnectAndRemoveSink,
                     base::Unretained(cast_media_sink_service_impl_), *sink),
      kExpirationDelay);
}

void AccessCodeCastSinkService::RemoveAndDisconnectExistingSinksOnNetwork() {
  for (auto& sink_id_keypair : current_session_expiration_timers_) {
    auto sink_id = sink_id_keypair.first;
    // If there is an active route for this sink -- don't attempt to remove it.
    // In this case we let the Media Router handle removals from the media
    // router when a network is changed with an active route.
    if (GetActiveRoute(sink_id).has_value()) {
      continue;
    }
    // We should remove `sinks_id` now because it is possible that the sink
    // service attempts to fetch from the pref service before
    // `RemoveAndDisconnectMediaSinkFromRouter()` is called. If `sink_id` is not
    // removed here, this sink might be considered a connected sink and the sink
    // service won't add it to the Media Router.

    // There are no active routes for this sink so it is safe to remove from the
    // media router. Must find the sink from media router for removal since it
    // has more total information.
    cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                       base::Unretained(cast_media_sink_service_impl_),
                       sink_id),
        base::BindOnce(
            &AccessCodeCastSinkService::RemoveAndDisconnectMediaSinkFromRouter,
            GetWeakPtr()));
  }
}

void AccessCodeCastSinkService::LogInfo(const std::string& log_message,
                                        const std::string& sink_id) {
  if (!media_router_ || !media_router_->GetLogger())
    return;
  media_router_->GetLogger()->LogInfo(mojom::LogCategory::kDiscovery,
                                      kLoggerComponent, log_message, sink_id,
                                      "", "");
}

void AccessCodeCastSinkService::LogWarning(const std::string& log_message,
                                           const std::string& sink_id) {
  if (!media_router_ || !media_router_->GetLogger())
    return;
  media_router_->GetLogger()->LogWarning(mojom::LogCategory::kDiscovery,
                                         kLoggerComponent, log_message, sink_id,
                                         "", "");
}

void AccessCodeCastSinkService::LogError(const std::string& log_message,
                                         const std::string& sink_id) {
  if (!media_router_ || !media_router_->GetLogger())
    return;
  media_router_->GetLogger()->LogError(mojom::LogCategory::kDiscovery,
                                       kLoggerComponent, log_message, sink_id,
                                       "", "");
}

void AccessCodeCastSinkService::OnNetworksChanged(
    const std::string& network_id) {
  RemoveAndDisconnectExistingSinksOnNetwork();
  // TODO: b/370067417 - Investigate if it is possible to remove this delay by
  // refactoring this file and/or media_router
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccessCodeCastSinkService::
                         ResetExpirationTimersAndInitAllStoredDevices,
                     GetWeakPtr()),
      kExpirationDelay + kNetworkChangeBuffer);
}

void AccessCodeCastSinkService::ResetExpirationTimersAndInitAllStoredDevices() {
  ResetExpirationTimers();
  InitAllStoredDevices();
}

void AccessCodeCastSinkService::OnDurationPrefChange() {
  ResetExpirationTimers();
  FetchAndValidateStoredDevices(
      base::BindOnce(&AccessCodeCastSinkService::InitExpirationTimers,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::OnEnabledPrefChange() {
  if (!GetAccessCodeCastEnabledPref(profile_)) {
    RemoveAndDisconnectExistingSinksOnNetwork();
    ResetExpirationTimers();
    if (pref_updater_) {
      pref_updater_->ClearDevicesDict(base::DoNothing());
      pref_updater_->ClearDeviceAddedTimeDict(base::DoNothing());
    }
  }
}

void AccessCodeCastSinkService::Shutdown() {
  network_monitor_->RemoveObserver(this);
  // There's no guarantee that MediaRouter is still in the
  // MediaRoutesObserver. |media_routes_observer_| accesses MediaRouter in its
  // dtor. Since MediaRouter and |this| are both KeyedServices, we must not
  // access MediaRouter in the dtor of |this|, so we do it here.
  media_routes_observer_.reset();
  if (user_prefs_registrar_)
    user_prefs_registrar_->RemoveAll();
  user_prefs_registrar_.reset();
  media_router_ = nullptr;
  ResetExpirationTimers();
}

void AccessCodeCastSinkService::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  DCHECK(identity_manager);
  identity_manager_ = identity_manager;
}

void AccessCodeCastSinkService::InitializePrefUpdater() {
  // If `pref_updater_` has been instantiated (i.e. for testing), do not
  // overwrite its value.
  if (!pref_updater_) {
    pref_updater_ = std::make_unique<AccessCodeCastPrefUpdaterImpl>(prefs_);
  }
  InitAllStoredDevices();
}

}  // namespace media_router
