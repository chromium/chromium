// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter_delegate.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/events/event.h"

SpokenFeedbackEventRewriterDelegate::SpokenFeedbackEventRewriterDelegate()
    : binding_(this) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                            &event_rewriter_controller_ptr_);
  // Set this object as the SpokenFeedbackEventRewriterDelegate.
  ash::mojom::SpokenFeedbackEventRewriterDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  event_rewriter_controller_ptr_->SetSpokenFeedbackEventRewriterDelegate(
      std::move(ptr));
}

SpokenFeedbackEventRewriterDelegate::~SpokenFeedbackEventRewriterDelegate() {}

void SpokenFeedbackEventRewriterDelegate::DispatchKeyEventToChromeVox(
    std::unique_ptr<ui::Event> event,
    bool capture) {
  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kChromeVoxExtensionId);
  if (!host)
    return;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  host->host_contents()->SetDelegate(capture ? this : nullptr);

  // Forward the event to ChromeVox's background page.
  chromeos::ForwardKeyToExtension(*(event->AsKeyEvent()), host);
}

void SpokenFeedbackEventRewriterDelegate::DispatchMouseEventToChromeVox(
    std::unique_ptr<ui::Event> event) {
  if (event->type() == ui::ET_MOUSE_MOVED) {
    AutomationManagerAura::GetInstance()->HandleEvent(
        ax::mojom::Event::kMouseMoved);
  }
}

bool SpokenFeedbackEventRewriterDelegate::ShouldDispatchKeyEventToChromeVox(
    const ui::Event* event) const {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager->IsSpokenFeedbackEnabled() ||
      accessibility_manager->keyboard_listener_extension_id().empty() ||
      !chromeos::GetAccessibilityExtensionHost(
          extension_misc::kChromeVoxExtensionId)) {
    VLOG(1) << "Event sent to Spoken Feedback when disabled or unavailable";
    return false;
  }

  if (!event || !event->IsKeyEvent()) {
    NOTREACHED() << "Unexpected event sent to Spoken Feedback";
    return false;
  }

  return true;
}

void SpokenFeedbackEventRewriterDelegate::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  event_rewriter_controller_ptr_->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

bool SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  OnUnhandledSpokenFeedbackEvent(ui::Event::Clone(*event.os_event));
  return true;
}
