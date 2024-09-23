// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace gfx {
class Size;
}

namespace extensions {

// Base class for platform-specific views used by extensions in the Chrome UI.
class ExtensionView {
 public:
  virtual ~ExtensionView() {}

  // Returns the extension's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  virtual void ResizeDueToAutoResize(content::WebContents* web_contents,
                                     const gfx::Size& new_size) = 0;

  // Method for the ExtensionHost to notify us when a renderer frame connection
  // is created.
  virtual void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) = 0;

  // Handles unhandled keyboard messages coming back from the renderer process.
  virtual bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) = 0;

  // Method for the ExtensionHost to notify that the extension page has loaded.
  virtual void OnLoaded() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
