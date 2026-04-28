// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include "chrome/browser/extensions/window_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "ui/base/base_window.h"

namespace windows_util {

// Android implementation of CalledFromChildWindow.
bool CalledFromChildWindow(ExtensionFunction* function,
                           const extensions::WindowController* controller) {
  content::WebContents* sender_web_contents = function->GetSenderWebContents();
  if (!sender_web_contents) {
    // Can't have been called by a child window if it wasn't called by a
    // WebContents.
    return false;
  }

  gfx::NativeWindow sender_native_window =
      sender_web_contents->GetTopLevelNativeWindow();

  gfx::NativeWindow controller_native_window =
      controller->window()->GetNativeWindow();

  // Unlike the views-based platforms (where we need to compare underlying
  // widgets), we can just compare native windows directly here. The native
  // window is a WindowAndroid*, which represents a ChromeActivity, and a child
  // window (like a toolbar popup) will have the same ChromeActivity as its
  // parent.
  return sender_native_window &&
         sender_native_window == controller_native_window;
}

}  // namespace windows_util
