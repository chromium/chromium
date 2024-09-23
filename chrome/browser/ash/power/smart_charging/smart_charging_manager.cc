// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/smart_charging/smart_charging_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/power/ml/recent_events_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace ash {
namespace power {

namespace {

using PastEvent = power_manager::PastChargingEvents::Event;
using PastChargingEvents = power_manager::PastChargingEvents;
using EventReason = power_manager::UserChargingEvent::Event::Reason;
using UserChargingEvent = power_manager::UserChargingEvent;

constexpr int kBucketSize = 15;

// Interval at which data should be logged.
constexpr auto kLoggingInterval = base::Minutes(30);

// Count number of key, mouse, touch events or duration of audio/video playing
// in the past 30 minutes.
constexpr auto kUserActivityDuration = base::Minutes(30);

// Granularity of input events is per minute.
constexpr int kNumUserInputEventsBuckets = kUserActivityDuration.InMinutes();

// Amount of time to wait between each try.
const base::TimeDelta kDelayBetweenTrials = base::Seconds(5);

constexpr char kSavedFileName[] = "past_charging_events.pb";
constexpr char kSavedDir[] = "smartcharging";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmartChargingMessage {
  kSerializeProtoError = 0,
  kWriteFileError = 1,
  kWriteFileSuccess = 2,
  kReadFileError = 3,
  kParseProtoError = 4,
  kReadFileSuccess = 5,
  kGetPrimaryProfileError = 6,
  kCreateFolderError = 7,
  kCreatePathSuccess = 8,
  kPathDoesntExists = 9,
  kGetPathSuccess = 10,
  kMaxValue = kGetPathSuccess
};

void LogSmartChargingMessage(SmartChargingMessage message) {
  UMA_HISTOGRAM_ENUMERATION("Power.SmartCharging.Messages", message);
}

// Given a proto and file path, writes to disk and logs the error(if any).
void WriteProtoToDisk(const PastChargingEvents& proto,
                      const base::FilePath& file_path) {
  std::string proto_string;
  if (!proto.SerializeToString(&proto_string)) {
    LogSmartChargingMessage(SmartChargingMessage::kSerializeProtoError);
    return;
  }
  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        file_path, proto_string.data(), "SmartCharging");
  }

  if (!write_result) {
    LogSmartChargingMessage(SmartChargingMessage::kWriteFileError);
    return;
  }
  LogSmartChargingMessage(SmartChargingMessage::kWriteFileSuccess);
}

// Reads a proto from a file path.
std::unique_ptr<PastChargingEvents> ReadProto(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string proto_str;
  if (!base::ReadFileToString(file_path, &proto_str)) {
    LogSmartChargingMessage(SmartChargingMessage::kReadFileError);
    return nullptr;
  }

  auto proto = std::make_unique<PastChargingEvents>();
  if (!proto->ParseFromString(proto_str)) {
    LogSmartChargingMessage(SmartChargingMessage::kParseProtoError);
    return nullptr;
  }
  LogSmartChargingMessage(SmartChargingMessage::kReadFileSuccess);
  return proto;
}

// If |create_new_path| is true, try to create new path and return true if
// success.
// If |create_new_path| is false, try to get the path and return true if
// sucesss.
bool GetPathSuccess(const base::FilePath profile_path,
                    base::FilePath* file_path,
                    bool create_new_path) {
  const base::FilePath path = profile_path.AppendASCII(kSavedDir);
  if (create_new_path) {
    if (!base::DirectoryExists(path) && !base::CreateDirectory(path)) {
      LogSmartChargingMessage(SmartChargingMessage::kCreateFolderError);
      return false;
    }
  } else if (!base::PathExists(path.AppendASCII(kSavedFileName))) {
    LogSmartChargingMessage(SmartChargingMessage::kPathDoesntExists);
    return false;
  }
  *file_path = path.AppendASCII(kSavedFileName);
  LogSmartChargingMessage(SmartChargingMessage::kGetPathSuccess);
  return true;
}

// Checks if an event is a halt (shutdown/suspend) event.
bool IsHaltEvent(const PastEvent& event) {
  return event.reason() == UserChargingEvent::Event::SHUTDOWN ||
         event.reason() == UserChargingEvent::Event::SUSPEND;
}

// Loads data from disk given a profile file path.
std::unique_ptr<PastChargingEvents> LoadFromDisk(
    const base::FilePath& profile_path) {
  base::FilePath file_path;
  std::unique_ptr<PastChargingEvents> proto;
  if (GetPathSuccess(profile_path, &file_path, false /*create_new_path*/)) {
    proto = ReadProto(file_path);
  }
  return proto;
}

// Saves data to disk given a profile file path.
void SaveToDisk(const std::vector<PastEvent>& past_events,
                const base::FilePath& profile_path) {
  base::FilePath file_path;
  if (GetPathSuccess(profile_path, &file_path, true /*create_new_path*/)) {
    PastChargingEvents proto;
    for (const auto& event : past_events) {
      *proto.add_events() = event;
    }
    WriteProtoToDisk(proto, file_path);
  }
}

}  // namespace

SmartChargingManager::SmartChargingManager(
    ui::UserActivityDetector* detector,
    mojo::PendingReceiver<viz::mojom::VideoDetectorObserver> receiver,
    session_manager::SessionManager* session_manager,
    std::unique_ptr<base::RepeatingTimer> periodic_timer)
    : periodic_timer_(std::move(periodic_timer)),
      receiver_(this, std::move(receiver)),
      mouse_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserActivityDuration,
          kNumUserInputEventsBuckets)),
      key_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserActivityDuration,
          kNumUserInputEventsBuckets)),
      stylus_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserActivityDuration,
          kNumUserInputEventsBuckets)),
      touch_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserActivityDuration,
          kNumUserInputEventsBuckets)),
      ukm_logger_(std::make_unique<SmartChargingUkmLogger>()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(detector);
  DCHECK(session_manager);
  user_activity_observation_.Observe(detector);
  power_manager_client_observation_.Observe(
      chromeos::PowerManagerClient::Get());
  session_manager_observation_.Observe(session_manager);
  blocking_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  UpdateChargeHistory();
  charge_history_timer_->Start(FROM_HERE, kLoggingInterval, this,
                               &SmartChargingManager::UpdateChargeHistory);
}

SmartChargingManager::~SmartChargingManager() = default;

std::unique_ptr<SmartChargingManager> SmartChargingManager::CreateInstance() {
  // TODO(crbug.com/40109338): we are collecting data from Chromebook only.
  // Since this action is discouraged, we will modify the condition latter using
  // dbus calls.
  if (chromeos::GetDeviceType() != chromeos::DeviceType::kChromebook)
    return nullptr;

  ui::UserActivityDetector* const detector = ui::UserActivityDetector::Get();
  DCHECK(detector);

  mojo::PendingRemote<viz::mojom::VideoDetectorObserver> video_observer;
  std::unique_ptr<SmartChargingManager> smart_charging_manager =
      std::make_unique<SmartChargingManager>(
          detector, video_observer.InitWithNewPipeAndPassReceiver(),
          session_manager::SessionManager::Get(),
          std::make_unique<base::RepeatingTimer>());

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(video_observer));

  return smart_charging_manager;
}

void SmartChargingManager::OnUserActivity(const ui::Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event)
    return;
  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();

  // Using time_since_boot instead of the event's time stamp so we can use the
  // boot clock.
  if (event->IsMouseEvent()) {
    mouse_counter_->Log(time_since_boot);
    return;
  }
  if (event->IsKeyEvent()) {
    key_counter_->Log(time_since_boot);
    return;
  }
  if (event->IsTouchEvent()) {
    if (event->AsTouchEvent()->pointer_details().pointer_type ==
        ui::EventPointerType::kPen) {
      stylus_counter_->Log(time_since_boot);
      return;
    }
    touch_counter_->Log(time_since_boot);
  }
}

void SmartChargingManager::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_brightness_percent_ = change.percent();
}

void SmartChargingManager::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }

  if (!proto.has_external_power()) {
    return;
  }

  // Logic for the first PowerChanged call.
  if (!external_power_.has_value()) {
    external_power_ = proto.external_power();
    periodic_timer_->Start(FROM_HERE, kLoggingInterval, this,
                           &SmartChargingManager::OnTimerFired);
    if (external_power_.value() == power_manager::PowerSupplyProperties::AC) {
      is_charging_ = true;
      LogEvent(UserChargingEvent::Event::CHARGER_PLUGGED_IN);
    } else {
      is_charging_ = false;
    }
    return;
  }

  const bool was_on_ac =
      external_power_.value() == power_manager::PowerSupplyProperties::AC;
  const bool now_on_ac =
      proto.external_power() == power_manager::PowerSupplyProperties::AC;

  is_charging_ = now_on_ac;

  // User plugged the charger in.
  if (!was_on_ac && now_on_ac) {
    external_power_ = proto.external_power();
    LogEvent(UserChargingEvent::Event::CHARGER_PLUGGED_IN);
  } else if (was_on_ac && !now_on_ac) {
    // User unplugged the charger.
    external_power_ = proto.external_power();
    LogEvent(UserChargingEvent::Event::CHARGER_UNPLUGGED);
  }
}

void SmartChargingManager::PowerManagerBecameAvailable(bool available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!available) {
    return;
  }
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&SmartChargingManager::OnReceiveScreenBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
  chromeos::PowerManagerClient::Get()->GetSwitchStates(
      base::BindOnce(&SmartChargingManager::OnReceiveSwitchStates,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartChargingManager::ShutdownRequested(
    power_manager::RequestShutdownReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogEvent(UserChargingEvent::Event::SHUTDOWN);
}

void SmartChargingManager::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogEvent(UserChargingEvent::Event::SUSPEND);
}

void SmartChargingManager::LidEventReceived(
    const chromeos::PowerManagerClient::LidState state,
    base::TimeTicks /* timestamp */) {
  lid_state_ = state;
}

void SmartChargingManager::TabletModeEventReceived(
    const chromeos::PowerManagerClient::TabletMode mode,
    base::TimeTicks /* timestamp */) {
  tablet_mode_ = mode;
}

void SmartChargingManager::OnVideoActivityStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  most_recent_video_start_time_ = boot_clock_.GetTimeSinceBoot();
  is_video_playing_ = true;
}

void SmartChargingManager::OnVideoActivityEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recent_video_usage_.push_back(TimePeriod(most_recent_video_start_time_,
                                           boot_clock_.GetTimeSinceBoot()));
  is_video_playing_ = false;
}

void SmartChargingManager::OnUserSessionStarted(bool /* is_primary_user */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The first sign-in user is the primary user, hence if |OnUserSessionStarted|
  // is called, the primary user profile should have been created. We will
  // ignore |is_primary_user|.
  if (loaded_from_disk_)
    return;
  if (!ProfileManager::GetPrimaryUserProfile()) {
    LogSmartChargingMessage(SmartChargingMessage::kGetPrimaryProfileError);
    return;
  }
  profile_path_ = ProfileManager::GetPrimaryUserProfile()->GetPath();
  MaybeLoadFromDisk(profile_path_.value());
}

void SmartChargingManager::PopulateUserChargingEventProto(
    UserChargingEvent* proto) {
  auto& features = *proto->mutable_features();
  if (battery_percent_)
    features.set_battery_percentage(static_cast<int>(battery_percent_.value()));

  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();
  features.set_num_recent_key_events(key_counter_->GetTotal(time_since_boot));
  features.set_num_recent_mouse_events(
      mouse_counter_->GetTotal(time_since_boot));
  features.set_num_recent_touch_events(
      touch_counter_->GetTotal(time_since_boot));
  features.set_num_recent_stylus_events(
      stylus_counter_->GetTotal(time_since_boot));

  if (is_charging_.has_value())
    features.set_is_charging(is_charging_.value());

  if (screen_brightness_percent_)
    features.set_screen_brightness_percent(
        static_cast<int>(screen_brightness_percent_.value()));

  features.set_duration_recent_video_playing_minutes(
      ukm::GetExponentialBucketMinForUserTiming(
          DurationRecentVideoPlaying().InMinutes()));

  // Set time related features.
  const base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  features.set_time_of_the_day_minutes(ukm::GetLinearBucketMin(
      static_cast<int64_t>(now_exploded.hour * 60 + now_exploded.minute),
      kBucketSize));
  features.set_day_of_week(static_cast<UserChargingEvent::Features::DayOfWeek>(
      now_exploded.day_of_week));
  features.set_day_of_month(now_exploded.day_of_month);
  features.set_month(
      static_cast<UserChargingEvent::Features::Month>(now_exploded.month));

  // Set device mode.
  if (lid_state_ == chromeos::PowerManagerClient::LidState::CLOSED) {
    features.set_device_mode(UserChargingEvent::Features::CLOSED_LID_MODE);
  } else if (tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
    features.set_device_mode(UserChargingEvent::Features::TABLET_MODE);
  } else if (lid_state_ == chromeos::PowerManagerClient::LidState::OPEN) {
    features.set_device_mode(UserChargingEvent::Features::LAPTOP_MODE);
  } else {
    features.set_device_mode(UserChargingEvent::Features::UNKNOWN_MODE);
  }

  // Last charge related features. This logic relies on the fact that
  // there will be at most one halt event because of |UpdatePastEvents()|.
  bool halt_from_last_charge = false;
  for (const auto& event : past_events_) {
    if (IsHaltEvent(event)) {
      halt_from_last_charge = true;
      break;
    }
  }
  features.set_halt_from_last_charge(halt_from_last_charge);

  PastEvent last_charge_plugged_in;
  PastEvent last_charge_unplugged;
  std::tie(last_charge_plugged_in, last_charge_unplugged) =
      GetLastChargeEvents();

  if (!last_charge_plugged_in.has_time() || !last_charge_unplugged.has_time())
    return;
  features.set_time_since_last_charge_minutes(
      ukm::GetExponentialBucketMinForCounts1000(
          now.ToDeltaSinceWindowsEpoch().InMinutes() -
          last_charge_unplugged.time()));
  features.set_duration_of_last_charge_minutes(
      ukm::GetExponentialBucketMinForCounts1000(last_charge_unplugged.time() -
                                                last_charge_plugged_in.time()));
  features.set_battery_percentage_before_last_charge(
      last_charge_plugged_in.battery_percent());
  features.set_battery_percentage_of_last_charge(
      last_charge_unplugged.battery_percent());
  features.set_timezone_difference_from_last_charge_hours(
      (now.UTCMidnight() - now.LocalMidnight()).InHours() -
      last_charge_unplugged.timezone());
}

void SmartChargingManager::LogEvent(const EventReason& reason) {
  UpdateChargeHistory();
  UserChargingEvent proto;
  proto.mutable_event()->set_event_id(++event_id_);
  proto.mutable_event()->set_reason(reason);
  PopulateUserChargingEventProto(&proto);

  // TODO(crbug.com/40109338): This is for testing only. Need to remove when
  // ukm logger is available.
  user_charging_event_for_test_ = proto;

  ukm_logger_->LogEvent(proto, charge_history_, base::Time::Now());

  AddPastEvent(reason);
  // Calls |UpdatePastEvents()| after |AddPastEvent()| to keep the number of
  // saved past events to be minimum.
  UpdatePastEvents();
  if (profile_path_.has_value()) {
    MaybeSaveToDisk(profile_path_.value());
  } else {
    LogSmartChargingMessage(SmartChargingMessage::kGetPrimaryProfileError);
  }
}

void SmartChargingManager::OnTimerFired() {
  LogEvent(power_manager::UserChargingEvent_Event::PERIODIC_LOG);
}

void SmartChargingManager::UpdateChargeHistory() {
  chromeos::PowerManagerClient::Get()->GetChargeHistoryForAdaptiveCharging(
      base::BindOnce(&SmartChargingManager::OnChargeHistoryReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartChargingManager::OnReceiveScreenBrightnessPercent(
    std::optional<double> screen_brightness_percent) {
  if (screen_brightness_percent.has_value()) {
    screen_brightness_percent_ = *screen_brightness_percent;
  }
}

void SmartChargingManager::OnReceiveSwitchStates(
    const std::optional<chromeos::PowerManagerClient::SwitchStates>
        switch_states) {
  if (switch_states.has_value()) {
    lid_state_ = switch_states->lid_state;
    tablet_mode_ = switch_states->tablet_mode;
  }
}

base::TimeDelta SmartChargingManager::DurationRecentVideoPlaying() {
  // Removes events that is out of |kUserActivityDuration|.
  const base::TimeDelta start_of_duration =
      boot_clock_.GetTimeSinceBoot() - kUserActivityDuration;
  while (!recent_video_usage_.empty() &&
         recent_video_usage_.front().end_time < start_of_duration) {
    recent_video_usage_.pop_front();
  }

  // Calculates total time.
  base::TimeDelta total_time = base::Seconds(0);
  for (const auto& event : recent_video_usage_) {
    total_time += std::min(event.end_time - event.start_time,
                           event.end_time - start_of_duration);
  }
  if (is_video_playing_) {
    total_time +=
        std::min(kUserActivityDuration, boot_clock_.GetTimeSinceBoot() -
                                            most_recent_video_start_time_);
  }
  return total_time;
}

void SmartChargingManager::MaybeLoadFromDisk(
    const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadFromDisk, profile_path),
      base::BindOnce(&SmartChargingManager::OnLoadProtoFromDiskComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartChargingManager::MaybeSaveToDisk(const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SaveToDisk, past_events_, profile_path));
}

void SmartChargingManager::OnLoadProtoFromDiskComplete(
    std::unique_ptr<PastChargingEvents> proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!proto) {
    return;
  }
  loaded_from_disk_ = true;
  for (const auto& event : proto.get()->events()) {
    past_events_.emplace_back(event);
  }
}

void SmartChargingManager::AddPastEvent(const EventReason& reason) {
  // Since we use |past_events_| to calculate last charge information, we don't
  // need to save if the reason is PERIODIC_LOG.
  if (reason == UserChargingEvent::Event::PERIODIC_LOG)
    return;
  PastEvent new_event;
  const base::Time now = base::Time::Now();
  new_event.set_time(now.ToDeltaSinceWindowsEpoch().InMinutes());
  if (battery_percent_.has_value())
    new_event.set_battery_percent(static_cast<int>(battery_percent_.value()));
  new_event.set_timezone((now.UTCMidnight() - now.LocalMidnight()).InHours());
  new_event.set_reason(reason);
  past_events_.emplace_back(new_event);
}

void SmartChargingManager::UpdatePastEvents() {
  PastEvent last_charge_plugged_in;
  PastEvent last_charge_unplugged;
  PastEvent new_plugged_in;
  PastEvent new_halt;

  std::tie(last_charge_plugged_in, last_charge_unplugged) =
      GetLastChargeEvents();
  if (last_charge_unplugged.has_time()) {
    // Gets the unplugged and halt(shutdown/suspend) events after the unplug (if
    // any).
    for (const auto& event : past_events_) {
      if (event.time() > last_charge_unplugged.time()) {
        if (event.reason() == UserChargingEvent::Event::CHARGER_PLUGGED_IN) {
          new_plugged_in = event;
        } else if (IsHaltEvent(event)) {
          new_halt = event;
        }
      }
    }
  } else {
    // Gets the last halt and plugged in event.
    for (const auto& event : past_events_) {
      if (event.reason() == UserChargingEvent::Event::CHARGER_PLUGGED_IN) {
        new_plugged_in = event;
      }
      if (IsHaltEvent(event)) {
        new_halt = event;
      }
    }
  }

  // Removes everything else.
  past_events_.clear();

  // Adds useful events back.
  if (last_charge_plugged_in.has_time())
    past_events_.emplace_back(last_charge_plugged_in);
  if (last_charge_unplugged.has_time())
    past_events_.emplace_back(last_charge_unplugged);
  if (new_plugged_in.has_time())
    past_events_.emplace_back(new_plugged_in);
  if (new_halt.has_time())
    past_events_.emplace_back(new_halt);
}

// Returns the last pair of plug/unplug events. If can't find the last pair,
// return a pair of empty events.
std::tuple<PastEvent, PastEvent> SmartChargingManager::GetLastChargeEvents() {
  PastEvent plugged_in;
  PastEvent unplugged;
  PastEvent temp_plugged_in;
  // There could be multiple events with CHARGER_PLUGGED_IN and/or
  // CHARGER_UNPLUGGED. This function relies on the fact that all events are
  // sorted by time.
  for (const auto& event : past_events_) {
    if (event.has_reason()) {
      if (event.reason() == UserChargingEvent::Event::CHARGER_PLUGGED_IN) {
        temp_plugged_in = event;
      } else if (event.reason() ==
                 UserChargingEvent::Event::CHARGER_UNPLUGGED) {
        if (!temp_plugged_in.has_time())
          continue;
        // Updates the pair of results.
        if (!plugged_in.has_time() ||
            temp_plugged_in.time() != plugged_in.time()) {
          plugged_in = temp_plugged_in;
          unplugged = event;
        }
      }
    }
  }
  return std::make_tuple(plugged_in, unplugged);
}

void SmartChargingManager::OnChargeHistoryReceived(
    std::optional<power_manager::ChargeHistoryState> proto) {
  if (proto.has_value()) {
    charge_history_ = proto.value();
    return;
  }
  // Retry if not get the charge history successfully
  if (num_trials_getting_charge_history_-- > 0) {
    retry_getting_charge_history_timer_->Start(
        FROM_HERE, kDelayBetweenTrials,
        base::BindOnce(&SmartChargingManager::UpdateChargeHistory,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace power
}  // namespace ash
