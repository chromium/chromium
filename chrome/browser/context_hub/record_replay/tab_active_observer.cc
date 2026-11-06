// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "url/gurl.h"

namespace context_hub {

TabActiveObserver::TabActiveObserver(SkillRecordingService* service,
                                     content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      service_(CHECK_DEREF(service)) {
  CHECK(web_contents);
}

TabActiveObserver::~TabActiveObserver() = default;

void TabActiveObserver::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown) {
    return;
  }
  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  const auto& mouse_event = static_cast<const blink::WebMouseEvent&>(event);
  if (mouse_event.button != blink::WebPointerProperties::Button::kLeft) {
    return;
  }
  const int x = base::ClampRound(mouse_event.PositionInWidget().x());
  const int y = base::ClampRound(mouse_event.PositionInWidget().y());
  service_->CaptureAction(
      browser::context_hub::mojom::SkillRecordingActionType::kLeftClick,
      base::StringPrintf("Mouse Clicked at X:%d, Y:%d", x, y));
}

}  // namespace context_hub
