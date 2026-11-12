// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include "base/feature_list.h"
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

namespace glic {

ShakeTrigger::ShakeTrigger(content::WebContents* web_contents,
                           ShakeTriggerClient& client)
    : web_contents_(web_contents->GetWeakPtr()),
      client_(client),
      detector_(CreateWiggleDetector(WiggleDetectorType::kLegacyShake)) {
  CHECK(detector_);
}

ShakeTrigger::~ShakeTrigger() = default;

void ShakeTrigger::OnInputEvent(const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseMove:
      ProcessMouseMove(static_cast<const blink::WebMouseEvent&>(event));
      break;
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      detector_->Reset();
      break;
    default:
      break;
  }
}

bool ShakeTrigger::IsEnabled() const {
  if (!base::FeatureList::IsEnabled(features::kGlicShakeTrigger)) {
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
  if (!IsEnabled()) {
    return;
  }
  if (detector_->AddSample(mouse_event.PositionInWidget(),
                           base::TimeTicks::Now())) {
    TriggerRegionCapture();
  }
}

}  // namespace glic
