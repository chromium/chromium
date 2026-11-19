// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/selection/selection_overlay_invocation_helper.h"
#include "chrome/browser/glic/selection/wiggle_detector.h"
#include "chrome/browser/glic/selection/wiggle_detector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/metrics_logger.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace glic {

namespace {

// How long after a count-only detection using glic counts as a follow-up.
constexpr base::TimeDelta kFollowUpWindow = base::Seconds(10);

constexpr base::FeatureParam<WiggleDetectorType>::Option kDetectorTypes[] = {
    {WiggleDetectorType::kLegacyShake, "legacy_shake"},
    {WiggleDetectorType::kLevel0, "level0"},
    {WiggleDetectorType::kLevel1, "level1"},
    {WiggleDetectorType::kLevel2, "level2"},
    {WiggleDetectorType::kLevel3, "level3"},
};

// The detector that triggers region capture.
const base::FeatureParam<WiggleDetectorType> kGlicShakeTriggerDetector{
    &features::kGlicShakeTrigger, "detector", WiggleDetectorType::kLegacyShake,
    &kDetectorTypes};

// Finch can override each value of the build's timing.
WiggleTiming GetWiggleTiming() {
  WiggleTiming timing = GetDefaultWiggleTiming();
  const base::Feature& feature = features::kGlicShakeTrigger;
  timing.cooldown = base::GetFieldTrialParamByFeatureAsTimeDelta(
      feature, "cooldown", timing.cooldown);
  timing.ignore_after_click = base::GetFieldTrialParamByFeatureAsTimeDelta(
      feature, "ignore_after_click", timing.ignore_after_click);
  timing.pause_threshold = base::GetFieldTrialParamByFeatureAsTimeDelta(
      feature, "pause_threshold", timing.pause_threshold);
  timing.ignore_after_pause = base::GetFieldTrialParamByFeatureAsTimeDelta(
      feature, "ignore_after_pause", timing.ignore_after_pause);
  return timing;
}

bool IsDragging(const blink::WebMouseEvent& mouse_event) {
  constexpr int kMouseButtonsDown = blink::WebInputEvent::kLeftButtonDown |
                                    blink::WebInputEvent::kMiddleButtonDown |
                                    blink::WebInputEvent::kRightButtonDown |
                                    blink::WebInputEvent::kBackButtonDown |
                                    blink::WebInputEvent::kForwardButtonDown;
  return mouse_event.GetModifiers() & kMouseButtonsDown;
}

WiggleDetectorType GetTriggerDetectorType() {
  WiggleDetectorType type = kGlicShakeTriggerDetector.Get();
  // Finch can pick a detector this build doesn't have, e.g. a non-branded
  // build signed in to a Google account.
  if (!std::ranges::contains(GetAvailableWiggleDetectorTypes(), type)) {
    return WiggleDetectorType::kLegacyShake;
  }
  return type;
}

base::span<const WiggleDetectorType> GetCountOnlyDetectorTypes() {
  if (!base::FeatureList::IsEnabled(features::kGlicShakeTrigger) ||
      !features::kGlicShakeTriggerShouldCount.Get()) {
    return {};
  }
  return GetAvailableWiggleDetectorTypes();
}

std::string_view GetDetectorTypeName(WiggleDetectorType type) {
  switch (type) {
    case WiggleDetectorType::kLegacyShake:
      return "LegacyShake";
    case WiggleDetectorType::kLevel0:
      return "Level0";
    case WiggleDetectorType::kLevel1:
      return "Level1";
    case WiggleDetectorType::kLevel2:
      return "Level2";
    case WiggleDetectorType::kLevel3:
      return "Level3";
  }
}

const void* const kActiveMouseMinuteTrackerKey = &kActiveMouseMinuteTrackerKey;

// Tracks the last counted active mouse minute across all tabs in a profile.
class ActiveMouseMinuteTracker : public base::SupportsUserData::Data {
 public:
  static void Record(Profile* profile,
                     base::TimeTicks timestamp,
                     const GlicMetricReporter& metric_reporter) {
    if (!profile) {
      return;
    }
    auto* tracker = static_cast<ActiveMouseMinuteTracker*>(
        profile->GetUserData(kActiveMouseMinuteTrackerKey));
    if (!tracker) {
      auto new_tracker = std::make_unique<ActiveMouseMinuteTracker>();
      tracker = new_tracker.get();
      profile->SetUserData(kActiveMouseMinuteTrackerKey,
                           std::move(new_tracker));
    }
    if (!tracker->minute_start_.is_null() &&
        timestamp - tracker->minute_start_ < base::Minutes(1)) {
      return;
    }
    tracker->minute_start_ = timestamp;
    metric_reporter.RecordBoolean("Wiggle.ActiveMouseMinutes", true);
  }

 private:
  base::TimeTicks minute_start_;
};

}  // namespace

ShakeTrigger::Detector::Detector(WiggleDetectorType type)
    : type(type), detector(CreateWiggleDetector(type)) {}
ShakeTrigger::Detector::Detector(Detector&&) noexcept = default;
ShakeTrigger::Detector& ShakeTrigger::Detector::operator=(Detector&&) noexcept =
    default;
ShakeTrigger::Detector::~Detector() = default;

ShakeTrigger::PendingFollowUp::PendingFollowUp(WiggleDetectorType type)
    : type(type) {}
ShakeTrigger::PendingFollowUp::~PendingFollowUp() = default;

ShakeTrigger::ShakeTrigger(content::WebContents* web_contents,
                           ShakeTriggerClient& client)
    : web_contents_(web_contents->GetWeakPtr()),
      client_(client),
      metric_reporter_(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      timing_(GetWiggleTiming()),
      trigger_detector_(GetTriggerDetectorType()) {
  for (WiggleDetectorType type : GetCountOnlyDetectorTypes()) {
    count_only_detectors_.emplace_back(type);
  }
}

ShakeTrigger::~ShakeTrigger() {
  for (const PendingFollowUp& pending : pending_follow_ups_) {
    RecordFollowedUp(pending.type, false);
  }
}

void ShakeTrigger::OnInputEvent(const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseMove:
      ProcessMouseMove(static_cast<const blink::WebMouseEvent&>(event));
      break;
    case blink::WebInputEvent::Type::kMouseUp:
      ResetDetectors();
      // Updates the click deadline. However mouse move could have already
      // bumped the deadline, hence std::max.
      ignore_until_ = std::max(
          ignore_until_, base::TimeTicks::Now() + timing_.ignore_after_click);
      break;
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      ResetDetectors();
      break;
    default:
      break;
  }
}

bool ShakeTrigger::IsEnabled() const {
  if (!base::FeatureList::IsEnabled(features::kGlicShakeTrigger) ||
      !features::kGlicShakeTriggerShouldTrigger.Get()) {
    return false;
  }
  if (!web_contents_) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  if (!profile->GetPrefs()->GetBoolean(prefs::kGlicShakeTriggerEnabled)) {
    return false;
  }
  if (features::kGlicShakeTriggerOnlyOnSidePanel.Get() &&
      !client_->IsSidePanelOpen()) {
    return false;
  }
  return true;
}

bool ShakeTrigger::ShouldObserveMouseMoves() const {
  return IsCounting() || IsEnabled();
}

bool ShakeTrigger::IsCounting() const {
  return !count_only_detectors_.empty();
}

void ShakeTrigger::OnFollowUpAction() {
  for (const PendingFollowUp& pending : pending_follow_ups_) {
    RecordFollowedUp(pending.type, true);
  }
  pending_follow_ups_.clear();
}

void ShakeTrigger::TriggerRegionCapture() {
  if (!client_->IsTextSelectionSharingEnabled() || !IsEnabled()) {
    return;
  }
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents_.get());
  if (!tab_interface) {
    return;
  }
  InvokeSelectionOverlay(*tab_interface,
                         ::selection::EntryPoint::kCursorWiggle);
}

void ShakeTrigger::ProcessMouseMove(const blink::WebMouseEvent& mouse_event) {
  bool trigger_enabled = IsEnabled();
  if (!trigger_enabled && count_only_detectors_.empty()) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  if (!count_only_detectors_.empty() && web_contents_) {
    ActiveMouseMinuteTracker::Record(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()), now,
        metric_reporter_);
  }
  bool moved_after_pause = !last_move_time_.is_null() &&
                           now - last_move_time_ >= timing_.pause_threshold;
  last_move_time_ = now;

  if (IsDragging(mouse_event)) {
    ResetDetectors();
    return;
  }
  if (moved_after_pause) {
    ResetDetectors();
    // Updates the mouse move deadline. However the mouse click might have
    // already bumped the deadline, hence std::max.
    ignore_until_ = std::max(ignore_until_, now + timing_.ignore_after_pause);
  }
  if (now < ignore_until_) {
    return;
  }

  gfx::PointF position = mouse_event.PositionInWidget();
  for (Detector& detector : count_only_detectors_) {
    if (AddSample(detector, position, now)) {
      RecordDetection(detector.type);
    }
  }
  if (trigger_enabled && AddSample(trigger_detector_, position, now)) {
    TriggerRegionCapture();
  }
}

bool ShakeTrigger::AddSample(Detector& detector,
                             gfx::PointF position,
                             base::TimeTicks timestamp) {
  if (timestamp < detector.cooldown_end) {
    return false;
  }
  if (!detector.detector->AddSample(position, timestamp)) {
    return false;
  }
  detector.cooldown_end = timestamp + timing_.cooldown;
  return true;
}

void ShakeTrigger::ResetDetectors() {
  trigger_detector_.detector->Reset();
  for (Detector& detector : count_only_detectors_) {
    detector.detector->Reset();
  }
}

void ShakeTrigger::RecordDetection(WiggleDetectorType type) {
  metric_reporter_.RecordEnumeration("Wiggle.Detected", type);
  // The trigger's own region capture would count as a follow-up.
  if (IsEnabled()) {
    return;
  }
  pending_follow_ups_.emplace_back(type).timer.Start(
      FROM_HERE, kFollowUpWindow, this, &ShakeTrigger::OnFollowUpWindowExpired);
}

void ShakeTrigger::OnFollowUpWindowExpired() {
  RecordFollowedUp(pending_follow_ups_.front().type, false);
  pending_follow_ups_.pop_front();
}

void ShakeTrigger::RecordFollowedUp(WiggleDetectorType type, bool followed) {
  metric_reporter_.RecordBoolean(
      base::StrCat({"Wiggle.FollowedUp.", GetDetectorTypeName(type)}),
      followed);
}

}  // namespace glic
