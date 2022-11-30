// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_EVENT_HANDLER_COMMON_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_EVENT_HANDLER_COMMON_H_

#include "extensions/browser/extension_host.h"
#include "ui/events/event_handler.h"

namespace ash {

// Gets the extension host for the corresponding extension ID.
extensions::ExtensionHost* GetAccessibilityExtensionHost(
    const std::string& extension_id);

// Forwards the key event to the extension background page for the
// corresponding host.
void ForwardKeyToExtension(const ui::KeyEvent& key_event,
                           extensions::ExtensionHost* host);

// Forwards the mouse event to the extension background page for the
// corresponding host.
void ForwardMouseToExtension(const ui::MouseEvent& mouse_event,
                             extensions::ExtensionHost* host);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_EVENT_HANDLER_COMMON_H_
