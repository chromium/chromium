// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/event_handler_common.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/blink/web_input_event.h"

namespace ash {

extensions::ExtensionHost* GetAccessibilityExtensionHost(
    const std::string& extension_id) {
  if (!AccessibilityManager::Get())
    return nullptr;

  content::BrowserContext* context = ProfileManager::GetActiveUserProfile();
  if (!context)
    return nullptr;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return nullptr;

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(context)->GetBackgroundHostForExtension(
          extension->id());
  return host;
}

void ForwardKeyToExtension(const ui::KeyEvent& key_event,
                           extensions::ExtensionHost* host) {
  if (!host) {
    VLOG(2) << "Unable to forward key to extension";
    return;
  }

  content::RenderFrameHost* main_frame = host->main_frame_host();
  DCHECK(main_frame);

  const input::NativeWebKeyboardEvent web_event(key_event);
  // Don't forward latency info, as these are getting forwarded to an extension.
  main_frame->GetRenderWidgetHost()->ForwardKeyboardEvent(web_event);
}

void ForwardMouseToExtension(const ui::MouseEvent& mouse_event,
                             extensions::ExtensionHost* host) {
  if (!host) {
    VLOG(2) << "Unable to forward mouse to extension";
    return;
  }

  content::RenderFrameHost* main_frame = host->main_frame_host();
  DCHECK(main_frame);

  if (mouse_event.type() == ui::EventType::kMouseExited) {
    VLOG(3) << "Couldn't forward unsupported mouse event to extension";
    return;
  }

  const blink::WebMouseEvent& web_event = ui::MakeWebMouseEvent(mouse_event);

  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    VLOG(3) << "Couldn't forward unsupported mouse event to extension";
    return;
  }

  // Don't forward latency info, as these are getting forwarded to an extension.
  main_frame->GetRenderWidgetHost()->ForwardMouseEvent(web_event);
}

}  // namespace ash
