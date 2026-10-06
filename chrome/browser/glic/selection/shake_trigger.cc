// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom-shared.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace glic {

namespace {

// Minimum distance in pixels required for a mouse move step to establish or
// change movement direction.
constexpr float kMinShakeDistance = 10.0f;
// Required number of direction changes to trigger region capture.
constexpr int kRequiredDirectionChanges = 4;
// Maximum time allowed between direction changes before the shake detector
// resets.
constexpr base::TimeDelta kShakeTimeout = base::Milliseconds(1000);

}  // namespace

ShakeTrigger::ShakeTrigger(content::WebContents* web_contents,
                           ShakeTriggerClient& client)
    : web_contents_(web_contents->GetWeakPtr()),
      client_(client),
      glic_keyed_service_(GlicKeyedService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {}

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
      Reset();
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
  if (!glic_keyed_service_) {
    return;
  }
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents_.get());
  if (!tab_interface) {
    return;
  }
  GlicInvokeOptions options(
      Target(*tab_interface),
      glic::mojom::InvocationSource::kCaptureRegionHotkey);
  options.wait_for_panel_open = true;
  glic_keyed_service_->Invoke(std::move(options));
}

void ShakeTrigger::ProcessMouseMove(const blink::WebMouseEvent& mouse_event) {
  if (!IsEnabled()) {
    return;
  }
  if (direction_change_count_ > 0 &&
      (base::TimeTicks::Now() - last_direction_change_time_) > kShakeTimeout) {
    Reset();
  }

  gfx::PointF current_pos = mouse_event.PositionInWidget();

  if (!last_shake_point_.has_value()) {
    last_shake_point_ = current_pos;
    return;
  }

  gfx::Vector2dF delta = current_pos - *last_shake_point_;
  float dist = delta.Length();
  if (dist < kMinShakeDistance) {
    return;
  }

  gfx::Vector2dF current_dir(delta.x() / dist, delta.y() / dist);

  if (!last_shake_dir_.has_value()) {
    last_shake_dir_ = current_dir;
    last_shake_point_ = current_pos;
    last_direction_change_time_ = base::TimeTicks::Now();
    return;
  }

  float dot = last_shake_dir_->x() * current_dir.x() +
              last_shake_dir_->y() * current_dir.y();
  if (dot < -0.5f) {
    direction_change_count_++;
    last_shake_dir_ = current_dir;
    last_shake_point_ = current_pos;
    last_direction_change_time_ = base::TimeTicks::Now();

    if (direction_change_count_ >= kRequiredDirectionChanges) {
      Reset();
      TriggerRegionCapture();
    }
  } else {
    last_shake_point_ = current_pos;
  }
}

void ShakeTrigger::Reset() {
  last_shake_point_.reset();
  last_shake_dir_.reset();
  direction_change_count_ = 0;
  last_direction_change_time_ = base::TimeTicks();
}

}  // namespace glic
