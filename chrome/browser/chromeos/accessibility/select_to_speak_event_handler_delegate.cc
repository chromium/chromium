// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler_delegate.h"

#include <memory>

#include "ash/public/cpp/accessibility_controller.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"

namespace chromeos {

SelectToSpeakEventHandlerDelegate::SelectToSpeakEventHandlerDelegate() {
  // Set this object as the SelectToSpeakEventHandlerDelegate.
  ash::AccessibilityController::Get()->SetSelectToSpeakEventHandlerDelegate(
      this);
}

SelectToSpeakEventHandlerDelegate::~SelectToSpeakEventHandlerDelegate() {
  if (auto* controller = ash::AccessibilityController::Get())
    controller->SetSelectToSpeakEventHandlerDelegate(nullptr);
}

void SelectToSpeakEventHandlerDelegate::DispatchKeyEvent(
    const ui::KeyEvent& event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  const ui::KeyEvent* key_event = event.AsKeyEvent();
  chromeos::ForwardKeyToExtension(*key_event, host);
}

void SelectToSpeakEventHandlerDelegate::DispatchMouseEvent(
    const ui::MouseEvent& event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  content::RenderViewHost* rvh = host->render_view_host();
  if (!rvh)
    return;

  rvh->GetWidget()->ForwardMouseEvent(ui::MakeWebMouseEvent(event));
}

}  // namespace chromeos
