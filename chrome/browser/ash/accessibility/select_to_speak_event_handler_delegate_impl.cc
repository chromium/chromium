// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/select_to_speak_event_handler_delegate_impl.h"

#include <memory>

#include "ash/public/cpp/accessibility_controller.h"
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

void SelectToSpeakEventHandlerDelegateImpl::DispatchKeyEvent(
    const ui::KeyEvent& event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  const ui::KeyEvent* key_event = event.AsKeyEvent();
  ForwardKeyToExtension(*key_event, host);
}

void SelectToSpeakEventHandlerDelegateImpl::DispatchMouseEvent(
    const ui::MouseEvent& event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // MouseWheel not handled by ui::MakeWebMouseEvent.
  if (event.type() == ui::EventType::ET_MOUSEWHEEL)
    return;

  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  content::RenderFrameHost* main_frame = host->main_frame_host();
  DCHECK(main_frame);

  main_frame->GetRenderWidgetHost()->ForwardMouseEvent(
      ui::MakeWebMouseEvent(event));
}

}  // namespace ash
