// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
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
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      network_monitor_(network_monitor),
      prefs_(prefs),
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
  std::set_difference(previous_routes_.begin(), previous_routes_.end(),
                      new_routes.begin(), new_routes.end(),
                      std::inserter(removed_routes, removed_routes.end()));

  std::vector<MediaRoute::Id> added_routes;
  std::set_difference(new_routes.begin(), new_routes.end(),
                      previous_routes_.begin(), previous_routes_.end(),
                      std::inserter(added_routes, removed_routes.end()));

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
  if (!IsSinkValidAccessCodeSink(sink))
    return;

  LogInfo("An Access Code Cast route has ended.", sink->id());

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
                            absl::nullopt);
    return;
  }
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    LogWarning(
        "We are not either not connected to a valid network or not connected "
        "to any network.",
        "");
    std::move(callback).Run(AddSinkResultCode::SERVICE_NOT_PRESENT,
                            absl::nullopt);
    return;
  }
  discovery_server_interface_ =
      std::make_unique<AccessCodeCastDiscoveryInterface>(
          profile_, access_code, media_router_->GetLogger(), identity_manager_);
  discovery_server_interface_->ValidateDiscoveryAccessCode(
      base::BindOnce(&AccessCodeCastSinkService::OnAccessCodeValidated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    LogInfo(
        "The sink already exists in the media router, no channel "
        "needs to be opened.",
        sink.id());

    // Only refresh data if this new sink came from typing in an access code.
    if (sink.cast_data().discovery_type ==
        CastDiscoveryType::kAccessCodeManualEntry) {
      // We can't store the sink by ID, since that will pull the outdated
      // information already in the media router.
      StoreSinkInPrefs(&sink);
      SetExpirationTimer(&sink);

      // Get the existing sink to we can update its info.
      cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::GetSinkById,
                         base::Unretained(cast_media_sink_service_impl_),
                         sink.id()),
          base::BindOnce(&AccessCodeCastSinkService::UpdateExistingSink,
                         weak_ptr_factory_.GetWeakPtr(), sink));
    }

    std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink.id());
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

absl::optional<const MediaRoute> AccessCodeCastSinkService::GetActiveRoute(
    const MediaSink::Id& sink_id) {
  if (!media_router_)
    return absl::nullopt;
  auto routes = media_router_->GetCurrentRoutes();
  auto route_it =
      base::ranges::find(routes, sink_id, &MediaRoute::media_sink_id);
  if (route_it == routes.end())
    return absl::nullopt;
  return *route_it;
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
    LogError("The channel failed to open.", sink_id);
    std::move(add_sink_callback)
        .Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, absl::nullopt);
    return;
  }
  LogInfo("The channel successfully opened.", sink_id);
  std::move(add_sink_callback).Run(AddSinkResultCode::OK, sink_id);
  StoreSinkAndSetExpirationTimer(sink_id);
}

void AccessCodeCastSinkService::StoreSinkAndSetExpirationTimer(
    const MediaSink::Id sink_id) {
  StoreSinkInPrefsById(sink_id);
  SetExpirationTimerById(sink_id);
}

void AccessCodeCastSinkService::CheckMediaSinkForExpiration(
    const MediaSink::Id& sink_id) {
  // Check to see if the sink is ready to be expired.
  if (!CalculateDurationTillExpiration(sink_id).is_zero())
    return;

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

void AccessCodeCastSinkService::StoreSinkInPrefsById(
    const MediaSink::Id sink_id) {
  cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
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
    LogError(
        "Unable to remember the cast sink since it was not present in the "
        "media router.",
        "");
    return;
  }
  pref_updater_->UpdateDevicesDict(*sink);
  pref_updater_->UpdateDeviceAddedTimeDict(sink->id());
}

void AccessCodeCastSinkService::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  DCHECK(identity_manager);
  identity_manager_ = identity_manager;
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
  cast_media_sink_service_impl_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
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
    LogError(
        "Unable to start an expiration timer for the cast sink since it was "
        "not present in the media router.",
        "");
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
    LogWarning(
        "We couldn't fetch the stored duration for some reason, default to "
        "instantly expiring this sink: " +
            sink_id,
        "");
    RemoveSinkIdFromAllEntries(sink_id);
    return base::Seconds(0);
  }

  base::Time time_of_expiration = fetched_device_added_time.value() +
                                  GetAccessCodeDeviceDurationPref(profile_);
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
      LogWarning(
          "The Media Sink id " + *sink_id_string +
              " is missing from one or more of the pref "
              "services. Attempting to remove all sink_id references right "
              "now.",
          "");
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
    LogInfo("There are no saved Access Code Cast devices for this profile.",
            "");
    return {};
  }
  LogInfo("Found Access Code Cast devices for this profile: " +
              sink_ids.DebugString() +
              ". Attempting to validate and then add these cast devices.",
          "");
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
  LogInfo("The sink id: " + sink.id() +
              " has expired. Checking to see if there is an active route, "
              "otherwise remove it from the media router and erase all stored "
              "references.",
          sink.id());

  auto route = GetActiveRoute(sink.id());
  // The given sink still has an active route, don't remove it yet and wait for
  // the route to end before we expire it.
  if (route.has_value() && route.value().is_local()) {
    LogInfo("The sink id: " + sink.id() +
                " still has a local route open. Wait to expire it until the "
                "route has "
                "ended.",
            sink.id());
    return;
  }

  ExpireSink(sink.id());
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
          weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastSinkService::RemoveAndDisconnectMediaSinkFromRouter(
    const MediaSinkInternal* sink) {
  if (!sink) {
    return;
  }

  // We don't want to remove a media sink that has an active route that is ALSO
  // a local route (casting the contents of this client).
  if (GetActiveRoute(sink->id()).has_value() &&
      GetActiveRoute(sink->id()).value().is_local())
    return;
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
  pref_updater_->RemoveSinkIdFromDevicesDict(sink_id);
  pref_updater_->RemoveSinkIdFromDeviceAddedTimeDict(sink_id);
}

absl::optional<const MediaSinkInternal>
AccessCodeCastSinkService::ValidateDeviceFromSinkId(
    const MediaSink::Id& sink_id) {
  const auto* sink_value =
      pref_updater_->GetMediaSinkInternalValueBySinkId(sink_id);
  if (!sink_value) {
    LogError(
        "The Media Sink id: " + sink_id +
            " is either stored improperly or doesn't exist within the pref "
            "service.",
        "");
    return absl::nullopt;
  }
  const auto* dict_value = sink_value->GetIfDict();
  if (!dict_value) {
    LogError(
        "The Media Sink id: " + sink_id +
            " was not stored as a dictionary value in the pref service. Its "
            "storage type is: " +
            base::Value::GetTypeName(sink_value->type()),
        "");
    return absl::nullopt;
  }
  const absl::optional<MediaSinkInternal> media_sink =
      ParseValueDictIntoMediaSinkInternal(*dict_value);
  if (!media_sink.has_value()) {
    LogError("The Media Sink " + dict_value->DebugString() +
                 " could not be parsed from the pref service.",
             "");
    return absl::nullopt;
  }

  return media_sink.value();
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
            weak_ptr_factory_.GetWeakPtr()));
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
  ResetExpirationTimers();
  InitAllStoredDevices();
}

void AccessCodeCastSinkService::OnDurationPrefChange() {
  ResetExpirationTimers();
  InitExpirationTimers(FetchAndValidateStoredDevices());
}

void AccessCodeCastSinkService::OnEnabledPrefChange() {
  if (!GetAccessCodeCastEnabledPref(profile_)) {
    RemoveAndDisconnectExistingSinksOnNetwork();
    ResetExpirationTimers();
    pref_updater_->ClearDevicesDict();
    pref_updater_->ClearDeviceAddedTimeDict();
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

}  // namespace media_router
