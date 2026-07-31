// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler_android.h"

#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

using protocol::Response;

namespace {
static constexpr char kNotImplemented[] = "Not implemented";
}  // namespace

// static
ui::BaseWindow* BrowserHandlerAndroid::FindBrowserWindowById(int window_id) {
  for (BrowserWindowInterface* bwi : GetAllBrowserWindowInterfaces()) {
    if (bwi->GetSessionID().id() == window_id) {
      return bwi->GetWindow();
    }
  }
  return nullptr;
}

// static
std::unique_ptr<protocol::Browser::Bounds>
BrowserHandlerAndroid::BuildBrowserWindowBounds(ui::BaseWindow* window) {
  bool is_minimized = window->IsMinimized();
  bool is_maximized = window->IsMaximized();

  // On non-resizable Android task-stack windows, "not currently visible" can
  // surface through ui::BaseWindow as minimized even though the app window is
  // still the maximized phone/tablet task. Keep minimized reporting for
  // resizable/freeform windows where minimized is a real window state.
  if (is_minimized) {
    ui::WindowResizePrecheckResult resize_precheck_result;
    if (!window->CanResize(resize_precheck_result)) {
      is_minimized = false;
      is_maximized = true;
    }
  }

  const std::string window_state = ComputeWindowStateString(
      window->IsFullscreen(), is_maximized, is_minimized);

  const gfx::Rect bounds =
      is_minimized ? window->GetRestoredBounds() : window->GetBounds();

  return protocol::Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(window_state)
      .Build();
}

BrowserHandlerAndroid::BrowserHandlerAndroid(
    protocol::UberDispatcher* dispatcher,
    const std::string& target_id)
    : target_id_(target_id) {
  CHECK(dispatcher);
  protocol::Browser::Dispatcher::wire(dispatcher, this);
}

BrowserHandlerAndroid::~BrowserHandlerAndroid() = default;

void BrowserHandlerAndroid::TrackBrowserWindow(
    BrowserWindowInterface* browser_window) {
  CHECK(browser_window);
  const SessionID& window_id = browser_window->GetSessionID();
  CHECK(window_id.is_valid());

  auto tracked_window = tracked_browser_windows_.find(window_id.id());
  if (tracked_window != tracked_browser_windows_.end() &&
      tracked_window->second != browser_window) {
    window_close_subscriptions_.erase(tracked_window->second);
  }
  tracked_browser_windows_.insert_or_assign(window_id.id(), browser_window);

  if (!window_close_subscriptions_.contains(browser_window)) {
    window_close_subscriptions_.insert_or_assign(
        browser_window,
        browser_window->RegisterBrowserDidClose(
            base::BindRepeating(&BrowserHandlerAndroid::OnBrowserWindowClosed,
                                base::Unretained(this))));
  }
}

Response BrowserHandlerAndroid::BuildBoundsForWindowId(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  ui::BaseWindow* window = FindBrowserWindowById(window_id);
  auto tracked_window = tracked_browser_windows_.find(window_id);
  if (window && tracked_window != tracked_browser_windows_.end()) {
    BrowserWindowInterface* browser_window = tracked_window->second;
    tracked_browser_windows_.erase(tracked_window);
    window_close_subscriptions_.erase(browser_window);
  } else if (!window && tracked_window != tracked_browser_windows_.end()) {
    window = tracked_window->second->GetWindow();
  }
  if (!window) {
    return Response::ServerError("Browser window not found");
  }

  *out_bounds = BuildBrowserWindowBounds(window);
  return Response::Success();
}

void BrowserHandlerAndroid::OnBrowserWindowClosed(
    BrowserWindowInterface* browser_window) {
  auto tracked_window =
      tracked_browser_windows_.find(browser_window->GetSessionID().id());
  if (tracked_window != tracked_browser_windows_.end() &&
      tracked_window->second == browser_window) {
    tracked_browser_windows_.erase(tracked_window);
  }
  window_close_subscriptions_.erase(browser_window);
}

// static
std::optional<int> BrowserHandlerAndroid::ResolveWindowIdForWebContents(
    content::WebContents* web_contents) {
  // Prefer the tab's own window id (covers pending ids).
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (tab) {
    SessionID wid = tab->GetWindowId();
    if (wid.is_valid()) {
      return wid.id();
    }
  }

  // Fallback: use the owning TabModel's session id.
  for (TabModel* model : TabModelList::models()) {
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* model_tab = model->GetTabAt(i);
      if (model_tab && model_tab->web_contents() == web_contents) {
        return model->GetSessionId().id();
      }
    }
  }

  return std::nullopt;
}

// static
std::string BrowserHandlerAndroid::ComputeWindowStateString(bool is_fullscreen,
                                                            bool is_maximized,
                                                            bool is_minimized) {
  if (is_fullscreen) {
    return "fullscreen";
  }
  if (is_maximized) {
    return "maximized";
  }
  if (is_minimized) {
    return "minimized";
  }
  return "normal";
}

Response BrowserHandlerAndroid::GetWindowForTarget(
    std::optional<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  auto host =
      content::DevToolsAgentHost::GetForId(target_id.value_or(target_id_));
  if (!host) {
    return Response::ServerError("No matching target");
  }
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents) {
    return Response::ServerError("No web contents in the target");
  }

  std::optional<int> window_id = ResolveWindowIdForWebContents(web_contents);
  if (!window_id.has_value()) {
    return Response::ServerError("Browser window not found");
  }
  *out_window_id = window_id.value();

  return BuildBoundsForWindowId(window_id.value(), out_bounds);
}

Response BrowserHandlerAndroid::GetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  return BuildBoundsForWindowId(window_id, out_bounds);
}

Response BrowserHandlerAndroid::Close() {
  return Response::MethodNotFound(kNotImplemented);
}

Response BrowserHandlerAndroid::SetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds> window_bounds) {
  return Response::MethodNotFound(kNotImplemented);
}

Response BrowserHandlerAndroid::SetContentsSize(int window_id,
                                                std::optional<int> width,
                                                std::optional<int> height) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::SetDockTile(
    std::optional<std::string> label,
    std::optional<protocol::Binary> image) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::ExecuteBrowserCommand(
    const protocol::Browser::BrowserCommandId& command_id) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::AddPrivacySandboxEnrollmentOverride(
    const std::string& in_url) {
  return Response::MethodNotFound(kNotImplemented);
}
