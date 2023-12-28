// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/select_to_speak_event_handler_delegate_impl.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"

namespace ash {

SelectToSpeakEventHandlerDelegateImpl::SelectToSpeakEventHandlerDelegateImpl() {
  // Set this object as the SelectToSpeakEventHandlerDelegate.
  AccessibilityController::Get()->SetSelectToSpeakEventHandlerDelegate(this);
}

SelectToSpeakEventHandlerDelegateImpl::
    ~SelectToSpeakEventHandlerDelegateImpl() {
  if (auto* controller = AccessibilityController::Get())
    controller->SetSelectToSpeakEventHandlerDelegate(nullptr);
}

void SelectToSpeakEventHandlerDelegateImpl::DispatchKeysCurrentlyDown(
    const std::set<ui::KeyboardCode>& pressed_keys) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_manager->SendKeysCurrentlyDownToSelectToSpeak(pressed_keys);
  }
}

void SelectToSpeakEventHandlerDelegateImpl::DispatchMouseEvent(
    const ui::MouseEvent& event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const gfx::PointF screen_point =
      event.target() ? event.target()->GetScreenLocationF(event)
                     : event.root_location_f();
  auto* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_manager->SendMouseEventToSelectToSpeak(event.type(),
                                                         screen_point);
  }
}

}  // namespace ash
