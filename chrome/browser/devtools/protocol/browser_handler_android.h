// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_

#include <optional>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/protocol/browser.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace ui {
class BaseWindow;
}

class BrowserHandlerAndroid : public protocol::Browser::Backend {
 public:
  BrowserHandlerAndroid(protocol::UberDispatcher* dispatcher,
                        const std::string& target_id);

  BrowserHandlerAndroid(const BrowserHandlerAndroid&) = delete;
  BrowserHandlerAndroid& operator=(const BrowserHandlerAndroid&) = delete;

  ~BrowserHandlerAndroid() override;

  // Tracks a window returned synchronously by CreateBrowserWindow() while
  // Android materializes its Activity. The close subscription bounds the raw
  // pointer's lifetime.
  void TrackBrowserWindow(BrowserWindowInterface* browser_window);

  static std::optional<int> ResolveWindowIdForWebContents(
      content::WebContents* web_contents);

  // Returns the ui::BaseWindow registered with the given window_id, or nullptr
  // if no registered AndroidBrowserWindow owns that session id.
  static ui::BaseWindow* FindBrowserWindowById(int window_id);

  // Builds a Browser::Bounds payload from the given window.
  static std::unique_ptr<protocol::Browser::Bounds> BuildBrowserWindowBounds(
      ui::BaseWindow* window);

  // Precedence: fullscreen > maximized > minimized > normal.
  static std::string ComputeWindowStateString(bool is_fullscreen,
                                              bool is_maximized,
                                              bool is_minimized);

  // Browser::Backend:
  protocol::Response GetWindowForTarget(
      std::optional<std::string> target_id,
      int* out_window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response GetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response Close() override;
  protocol::Response SetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds> window_bounds) override;
  protocol::Response SetContentsSize(int window_id,
                                     std::optional<int> width,
                                     std::optional<int> height) override;
  protocol::Response SetDockTile(
      std::optional<std::string> label,
      std::optional<protocol::Binary> image) override;
  protocol::Response ExecuteBrowserCommand(
      const protocol::Browser::BrowserCommandId& command_id) override;
  protocol::Response AddPrivacySandboxEnrollmentOverride(
      const std::string& in_url) override;

 private:
  struct WindowLookupResult {
    raw_ptr<ui::BaseWindow> window;
    bool is_pending;
  };

  WindowLookupResult FindWindowById(int window_id);

  protocol::Response BuildBoundsForWindowId(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds);
  void OnBrowserWindowClosed(BrowserWindowInterface* browser_window);

  const std::string target_id_;
  base::flat_map<int, raw_ptr<BrowserWindowInterface>> tracked_browser_windows_;
  base::flat_map<BrowserWindowInterface*, base::CallbackListSubscription>
      window_close_subscriptions_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_
