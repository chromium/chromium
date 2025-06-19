// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include "base/notimplemented.h"
#include "content/public/browser/keyboard_event_processing_result.h"

using content::WebContents;

content::KeyboardEventProcessingResult DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

std::unique_ptr<content::EyeDropper> DevToolsWindow::OpenEyeDropper(
    content::RenderFrameHost* render_frame_host,
    content::EyeDropperListener* listener) {
  NOTIMPLEMENTED();
  return nullptr;
}

void DevToolsWindow::UpdateBrowserToolbar() {
  NOTIMPLEMENTED();
}

void DevToolsWindow::UpdateBrowserWindow() {
  NOTIMPLEMENTED();
}
