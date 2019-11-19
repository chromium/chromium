// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_manager.h"

#include <cmath>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger_impl.h"
#include "chrome/browser/chromeos/power/ml/recent_events_counter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/aura/env.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {
// Count number of key, mouse and touch events in the past hour.
constexpr base::TimeDelta kUserInputEventsDuration =
    base::TimeDelta::FromMinutes(60);

// Granularity of input events is per minute.
constexpr int kNumUserInputEventsBuckets =
    kUserInputEventsDuration / base::TimeDelta::FromMinutes(1);

// Returns the focused visible browser unless no visible browser is focused,
// then returns the topmost visible browser.
// Returns nullopt if no suitable browsers are found.
Browser* GetFocusedOrTopmostVisibleBrowser() {
  Browser* topmost_browser = nullptr;
  Browser* browser = nullptr;
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  for (auto browser_iterator = browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    browser = *browser_iterator;
    if (browser->profile()->IsOffTheRecord() || !browser->window()->IsVisible())
      continue;

    if (browser->window()->IsActive())
      return browser;

    if (!topmost_browser)
      topmost_browser = *browser_iterator;
  }
  if (topmost_browser)
    return topmost_browser;

  return nullptr;
}

// For the active tab, returns the UKM SourceId and whether any form in this
// tab had an interaction. The active tab is in the focused visible browser.
// If no visible browser is focused, the topmost visible browser is used.
const std::pair<ukm::SourceId, bool> GetActiveTabData() {
  ukm::SourceId tab_id = ukm::kInvalidSourceId;
  bool has_form_entry = false;
  Browser* browser = GetFocusedOrTopmostVisibleBrowser();
  if (browser) {
    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);
    content::WebContents* contents = tab_strip_model->GetActiveWebContents();
    if (contents) {
      tab_id = ukm::GetSourceIdForWebContentsDocument(contents);
      has_form_entry =
          contents->GetPageImportanceSignals().had_form_interaction;
    }
  }
  return std::pair<ukm::SourceId, bool>(tab_id, has_form_entry);
}

}  // namespace

constexpr base::TimeDelta AdaptiveScreenBrightnessManager::kInactivityDuration;
constexpr base::TimeDelta AdaptiveScreenBrightnessManager::kLoggingInterval;

AdaptiveScreenBrightnessManager::AdaptiveScreenBrightnessManager(
    std::unique_ptr<AdaptiveScreenBrightnessUkmLogger> ukm_logger,
    ui::UserActivityDetector* detector,
    chromeos::PowerManagerClient* power_manager_client,
    AccessibilityManager* accessibility_manager,
    MagnificationManager* magnification_manager,
    mojo::PendingReceiver<viz::mojom::VideoDetectorObserver> receiver,
    std::unique_ptr<base::RepeatingTimer> periodic_timer)
    : periodic_timer_(std::move(periodic_timer)),
      ukm_logger_(std::move(ukm_logger)),
      accessibility_manager_(accessibility_manager),
      magnification_manager_(magnification_manager),
      receiver_(this, std::move(receiver)),
      mouse_counter_(
          std::make_unique<RecentEventsCounter>(kUserInputEventsDuration,
                                                kNumUserInputEventsBuckets)),
      key_counter_(
          std::make_unique<RecentEventsCounter>(kUserInputEventsDuration,
                                                kNumUserInputEventsBuckets)),
      stylus_counter_(
          std::make_unique<RecentEventsCounter>(kUserInputEventsDuration,
                                                kNumUserInputEventsBuckets)),
      touch_counter_(
          std::make_unique<RecentEventsCounter>(kUserInputEventsDuration,
                                                kNumUserInputEventsBuckets)) {
  DCHECK(ukm_logger_);
  DCHECK(detector);
  user_activity_observer_.Add(detector);

  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();
  power_manager_client->GetSwitchStates(
      base::BindOnce(&AdaptiveScreenBrightnessManager::OnReceiveSwitchStates,
                     weak_ptr_factory_.GetWeakPtr()));
  power_manager_client->GetScreenBrightnessPercent(base::BindOnce(
      &AdaptiveScreenBrightnessManager::OnReceiveScreenBrightnessPercent,
      weak_ptr_factory_.GetWeakPtr()));
  DCHECK(periodic_timer_);
  periodic_timer_->Start(FROM_HERE, kLoggingInterval, this,
                         &AdaptiveScreenBrightnessManager::OnTimerFired);
}

AdaptiveScreenBrightnessManager::~AdaptiveScreenBrightnessManager() = default;

std::unique_ptr<AdaptiveScreenBrightnessManager>
AdaptiveScreenBrightnessManager::CreateInstance() {
  if (chromeos::GetDeviceType() != chromeos::DeviceType::kChromebook)
    return nullptr;

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  ui::UserActivityDetector* const detector = ui::UserActivityDetector::Get();
  DCHECK(detector);
  AccessibilityManager* const accessibility_manager =
      AccessibilityManager::Get();
  DCHECK(accessibility_manager);
  MagnificationManager* const magnification_manager =
      MagnificationManager::Get();
  DCHECK(magnification_manager);
  mojo::PendingRemote<viz::mojom::VideoDetectorObserver>
      video_observer_screen_brightness_logger;

  std::unique_ptr<AdaptiveScreenBrightnessManager> screen_brightness_manager =
      std::make_unique<AdaptiveScreenBrightnessManager>(
          std::make_unique<AdaptiveScreenBrightnessUkmLoggerImpl>(), detector,
          power_manager_client, accessibility_manager, magnification_manager,
          video_observer_screen_brightness_logger
              .InitWithNewPipeAndPassReceiver(),
          std::make_unique<base::RepeatingTimer>());
  aura::Env::GetInstance()
      ->context_factory_private()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(
          std::move(video_observer_screen_brightness_logger));

  return screen_brightness_manager;
}

void AdaptiveScreenBrightnessManager::OnUserActivity(
    const ui::Event* const event) {
  if (!event)
    return;
  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();
  // Update |start_activity_time_since_boot_| if the time since the last
  // activity is at least kInactivityDuration. An absense of activity for this
  // length of time indicates that one activity period has ended and the next
  // activity period can begin. There is no update if video is playing, as
  // playing a video counts as ongoing activity.
  if ((!last_activity_time_since_boot_.has_value() ||
       time_since_boot - *last_activity_time_since_boot_ >=
           kInactivityDuration) &&
      !is_video_playing_.value_or(false)) {
    start_activity_time_since_boot_ = time_since_boot;
  }
  last_activity_time_since_boot_ = time_since_boot;

  // Using time_since_boot instead of the event's time stamp so we can use the
  // boot clock.
  if (event->IsMouseEvent()) {
    mouse_counter_->Log(time_since_boot);
  } else if (event->IsKeyEvent()) {
    key_counter_->Log(time_since_boot);
  } else if (event->IsTouchEvent()) {
    if (event->AsTouchEvent()->pointer_details().pointer_type ==
        ui::EventPointerType::POINTER_TYPE_PEN) {
      stylus_counter_->Log(time_since_boot);
    } else {
      touch_counter_->Log(time_since_boot);
    }
  }
}

void AdaptiveScreenBrightnessManager::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  switch (change.cause()) {
    case power_manager::BacklightBrightnessChange_Cause_USER_REQUEST:
      if (screen_brightness_percent_.has_value()) {
        reason_ = (screen_brightness_percent_ < change.percent())
                      ? ScreenBrightnessEvent::Event::USER_UP
                      : ScreenBrightnessEvent::Event::USER_DOWN;
      } else {
        reason_ = ScreenBrightnessEvent::Event::OTHER;
      }
      break;
    case power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY:
      reason_ = ScreenBrightnessEvent::Event::USER_ACTIVITY;
      break;
    case power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY:
      reason_ = change.percent() == 0
                    ? ScreenBrightnessEvent::Event::OFF_FOR_INACTIVITY
                    : ScreenBrightnessEvent::Event::DIMMED_FOR_INACTIVITY;
      break;
    case power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED:
      reason_ = ScreenBrightnessEvent::Event::AMBIENT_LIGHT_CHANGED;
      break;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED:
      reason_ = ScreenBrightnessEvent::Event::EXTERNAL_POWER_CONNECTED;
      break;
    case power_manager::
        BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED:
      reason_ = ScreenBrightnessEvent::Event::EXTERNAL_POWER_DISCONNECTED;
      break;
    case power_manager::BacklightBrightnessChange_Cause_FORCED_OFF:
      reason_ = ScreenBrightnessEvent::Event::FORCED_OFF;
      break;
    case power_manager::BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF:
      reason_ = ScreenBrightnessEvent::Event::NO_LONGER_FORCED_OFF;
      break;
    case power_manager::BacklightBrightnessChange_Cause_OTHER:
    default:
      reason_ = ScreenBrightnessEvent::Event::OTHER;
  }
  previous_screen_brightness_percent_ = screen_brightness_percent_;
  screen_brightness_percent_ = change.percent();
  LogEvent();
}

void AdaptiveScreenBrightnessManager::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  external_power_ = proto.external_power();

  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }
}

void AdaptiveScreenBrightnessManager::LidEventReceived(
    const chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& /* timestamp */) {
  lid_state_ = state;
}

void AdaptiveScreenBrightnessManager::TabletModeEventReceived(
    const chromeos::PowerManagerClient::TabletMode mode,
    const base::TimeTicks& /* timestamp */) {
  tablet_mode_ = mode;
}

void AdaptiveScreenBrightnessManager::OnVideoActivityStarted() {
  is_video_playing_ = true;
  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();
  if (!last_activity_time_since_boot_.has_value() ||
      time_since_boot - *last_activity_time_since_boot_ >=
          kInactivityDuration) {
    start_activity_time_since_boot_ = time_since_boot;
  }
  last_activity_time_since_boot_ = time_since_boot;
}

void AdaptiveScreenBrightnessManager::OnVideoActivityEnded() {
  is_video_playing_ = false;
  last_activity_time_since_boot_ = boot_clock_.GetTimeSinceBoot();
}

void AdaptiveScreenBrightnessManager::OnTimerFired() {
  reason_ = ScreenBrightnessEvent::Event::PERIODIC;
  previous_screen_brightness_percent_ = screen_brightness_percent_;
  LogEvent();
}

void AdaptiveScreenBrightnessManager::OnReceiveSwitchStates(
    const base::Optional<chromeos::PowerManagerClient::SwitchStates>
        switch_states) {
  if (switch_states.has_value()) {
    lid_state_ = switch_states->lid_state;
    tablet_mode_ = switch_states->tablet_mode;
  }
}

void AdaptiveScreenBrightnessManager::OnReceiveScreenBrightnessPercent(
    const base::Optional<double> screen_brightness_percent) {
  if (screen_brightness_percent.has_value()) {
    previous_screen_brightness_percent_ = screen_brightness_percent_;
    screen_brightness_percent_ = *screen_brightness_percent;
  }
}

const base::Optional<int>
AdaptiveScreenBrightnessManager::GetNightLightTemperaturePercent() const {
  const Profile* const profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return base::nullopt;

  const PrefService* const pref_service = profile->GetPrefs();
  if (!pref_service)
    return base::nullopt;

  if (!pref_service->GetBoolean(ash::prefs::kNightLightEnabled))
    return base::nullopt;
  return std::floor(
      pref_service->GetDouble(ash::prefs::kNightLightTemperature) * 100);
}

void AdaptiveScreenBrightnessManager::LogEvent() {
  if (!screen_brightness_percent_.has_value()) {
    return;
  }

  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();

  ScreenBrightnessEvent screen_brightness;
  ScreenBrightnessEvent::Event* const event = screen_brightness.mutable_event();
  event->set_brightness(*screen_brightness_percent_);
  if (reason_.has_value()) {
    event->set_reason(*reason_);
    reason_ = base::nullopt;
  }
  if (last_event_time_since_boot_.has_value()) {
    event->set_time_since_last_event_sec(
        (time_since_boot - *last_event_time_since_boot_).InSeconds());
  }

  ScreenBrightnessEvent::Features* const features =
      screen_brightness.mutable_features();

  ScreenBrightnessEvent::Features::ActivityData* const activity_data =
      features->mutable_activity_data();

  const base::Time now = base::Time::Now();
  activity_data->set_time_of_day_sec((now - now.LocalMidnight()).InSeconds());

  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  activity_data->set_day_of_week(
      static_cast<ScreenBrightnessEvent_Features_ActivityData_DayOfWeek>(
          exploded.day_of_week));

  activity_data->set_num_recent_mouse_events(
      mouse_counter_->GetTotal(time_since_boot));
  activity_data->set_num_recent_key_events(
      key_counter_->GetTotal(time_since_boot));
  activity_data->set_num_recent_stylus_events(
      stylus_counter_->GetTotal(time_since_boot));
  activity_data->set_num_recent_touch_events(
      touch_counter_->GetTotal(time_since_boot));

  if (is_video_playing_.value_or(false)) {
    activity_data->set_last_activity_time_sec(0);
    DCHECK(start_activity_time_since_boot_);
    activity_data->set_recent_time_active_sec(
        (time_since_boot - *start_activity_time_since_boot_).InSeconds());
  } else if (last_activity_time_since_boot_.has_value()) {
    activity_data->set_last_activity_time_sec(
        (time_since_boot - *last_activity_time_since_boot_).InSeconds());
    if (start_activity_time_since_boot_.has_value()) {
      activity_data->set_recent_time_active_sec(
          (*last_activity_time_since_boot_ - *start_activity_time_since_boot_)
              .InSeconds());
    }
  }

  if (is_video_playing_.has_value()) {
    activity_data->set_is_video_playing(*is_video_playing_);
  }

  if (battery_percent_.has_value()) {
    features->mutable_env_data()->set_battery_percent(*battery_percent_);
  }
  if (external_power_.has_value()) {
    features->mutable_env_data()->set_on_battery(
        *external_power_ == power_manager::PowerSupplyProperties::DISCONNECTED);
  }

  // Set device mode.
  if (lid_state_ == chromeos::PowerManagerClient::LidState::CLOSED) {
    features->mutable_env_data()->set_device_mode(
        ScreenBrightnessEvent::Features::EnvData::CLOSED_LID);
  } else if (lid_state_ == chromeos::PowerManagerClient::LidState::OPEN) {
    if (tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
      features->mutable_env_data()->set_device_mode(
          ScreenBrightnessEvent::Features::EnvData::TABLET);
    } else {
      features->mutable_env_data()->set_device_mode(
          ScreenBrightnessEvent::Features::EnvData::LAPTOP);
    }
  } else {
    features->mutable_env_data()->set_device_mode(
        ScreenBrightnessEvent::Features::EnvData::UNKNOWN_MODE);
  }

  const base::Optional<int> temperature = GetNightLightTemperaturePercent();
  if (temperature.has_value()) {
    features->mutable_env_data()->set_night_light_temperature_percent(
        *temperature);
  }

  if (previous_screen_brightness_percent_.has_value()) {
    features->mutable_env_data()->set_previous_brightness(
        *previous_screen_brightness_percent_);
  }

  ScreenBrightnessEvent::Features::AccessibilityData* const accessibility_data =
      features->mutable_accessibility_data();

  if (accessibility_manager_) {
    accessibility_data->set_is_high_contrast_enabled(
        accessibility_manager_->IsHighContrastEnabled());
    accessibility_data->set_is_large_cursor_enabled(
        accessibility_manager_->IsLargeCursorEnabled());
    accessibility_data->set_is_virtual_keyboard_enabled(
        accessibility_manager_->IsVirtualKeyboardEnabled());
    accessibility_data->set_is_spoken_feedback_enabled(
        accessibility_manager_->IsSpokenFeedbackEnabled());
    accessibility_data->set_is_select_to_speak_enabled(
        accessibility_manager_->IsSelectToSpeakEnabled());
    accessibility_data->set_is_mono_audio_enabled(
        accessibility_manager_->IsMonoAudioEnabled());
    accessibility_data->set_is_caret_highlight_enabled(
        accessibility_manager_->IsCaretHighlightEnabled());
    accessibility_data->set_is_cursor_highlight_enabled(
        accessibility_manager_->IsCursorHighlightEnabled());
    accessibility_data->set_is_focus_highlight_enabled(
        accessibility_manager_->IsFocusHighlightEnabled());
    accessibility_data->set_is_braille_display_connected(
        accessibility_manager_->IsBrailleDisplayConnected());
    accessibility_data->set_is_autoclick_enabled(
        accessibility_manager_->IsAutoclickEnabled());
    accessibility_data->set_is_switch_access_enabled(
        accessibility_manager_->IsSwitchAccessEnabled());
  }

  if (magnification_manager_) {
    accessibility_data->set_is_magnifier_enabled(
        magnification_manager_->IsMagnifierEnabled());
  }

  const std::pair<ukm::SourceId, bool> tab_data = GetActiveTabData();

  // Log to metrics.
  ukm_logger_->LogActivity(screen_brightness, tab_data.first, tab_data.second);

  last_event_time_since_boot_ = time_since_boot;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
