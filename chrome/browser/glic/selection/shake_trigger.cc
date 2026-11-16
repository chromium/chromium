// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
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

std::unique_ptr<WiggleDetector> CreateTriggerDetector() {
  WiggleDetectorType type = kGlicShakeTriggerDetector.Get();
  // Finch can pick a detector this build doesn't have, e.g. a non-branded
  // build signed in to a Google account.
  if (!std::ranges::contains(GetAvailableWiggleDetectorTypes(), type)) {
    type = WiggleDetectorType::kLegacyShake;
  }
  return CreateWiggleDetector(type);
}

std::vector<std::unique_ptr<WiggleDetector>> CreateCountOnlyDetectors() {
  std::vector<std::unique_ptr<WiggleDetector>> detectors;
  if (!base::FeatureList::IsEnabled(features::kGlicShakeTrigger) ||
      !features::kGlicShakeTriggerShouldCount.Get()) {
    return detectors;
  }
  for (WiggleDetectorType type : GetAvailableWiggleDetectorTypes()) {
    detectors.push_back(CreateWiggleDetector(type));
  }
  return detectors;
}

}  // namespace

ShakeTrigger::Detector::Detector(std::unique_ptr<WiggleDetector> detector)
    : detector(std::move(detector)) {}
ShakeTrigger::Detector::Detector(Detector&&) = default;
ShakeTrigger::Detector& ShakeTrigger::Detector::operator=(Detector&&) = default;
ShakeTrigger::Detector::~Detector() = default;

ShakeTrigger::ShakeTrigger(content::WebContents* web_contents,
                           ShakeTriggerClient& client)
    : web_contents_(web_contents->GetWeakPtr()),
      client_(client),
      timing_(GetWiggleTiming()),
      trigger_detector_(CreateTriggerDetector()) {
  for (auto& detector : CreateCountOnlyDetectors()) {
    count_only_detectors_.emplace_back(std::move(detector));
  }
}

ShakeTrigger::~ShakeTrigger() = default;

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
  return !count_only_detectors_.empty() || IsEnabled();
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
    AddSample(detector, position, now);
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

}  // namespace glic
