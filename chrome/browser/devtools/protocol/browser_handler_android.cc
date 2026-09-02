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
#include "components/sessions/core/session_id.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

using protocol::Response;

namespace {
static constexpr char kNotImplemented[] = "Not implemented";

struct ResolvedWindowState {
  std::string name;
  bool is_minimized;
};

ResolvedWindowState ResolveWindowState(ui::BaseWindow* window) {
  bool is_minimized = window->IsMinimized();
  bool is_maximized = window->IsMaximized();

  // A backgrounded fixed-size Android task is not minimized in the CDP sense.
  if (is_minimized) {
    ui::WindowResizePrecheckResult resize_precheck_result;
    if (!window->CanResize(resize_precheck_result) &&
        resize_precheck_result !=
            ui::WindowResizePrecheckResult::kAndroidNoActivity) {
      is_minimized = false;
      is_maximized = true;
    }
  }

  return {
      BrowserHandlerAndroid::ComputeWindowStateString(
          window->IsFullscreen(), is_maximized, is_minimized),
      is_minimized,
  };
}

// CDP window-state mutations are only observable for resizable Android tasks.
// Fixed-size tasks continue to report maximized even when backgrounded.
Response CheckCanChangeWindow(ui::BaseWindow* window, bool is_pending) {
  ui::WindowResizePrecheckResult resize_precheck_result;
  if (window->CanResize(resize_precheck_result) ||
      (is_pending && resize_precheck_result ==
                         ui::WindowResizePrecheckResult::kAndroidNoActivity)) {
    // A DevTools-created Android task can be addressed before its Activity is
    // attached. ChromeAndroidTask queues state and bounds changes in this case.
    return Response::Success();
  }
  return Response::ServerError(
      "Window state or bounds cannot be changed in the current Android "
      "configuration");
}
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
  ResolvedWindowState window_state = ResolveWindowState(window);

  const gfx::Rect bounds = window_state.is_minimized
                               ? window->GetRestoredBounds()
                               : window->GetBounds();

  return protocol::Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(window_state.name)
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
  auto [window, _] = FindWindowById(window_id);
  if (!window) {
    return Response::ServerError("Browser window not found");
  }

  *out_bounds = BuildBrowserWindowBounds(window);
  return Response::Success();
}

BrowserHandlerAndroid::WindowLookupResult BrowserHandlerAndroid::FindWindowById(
    int window_id) {
  if (ui::BaseWindow* window = FindBrowserWindowById(window_id)) {
    auto tracked_window = tracked_browser_windows_.find(window_id);
    if (tracked_window != tracked_browser_windows_.end()) {
      BrowserWindowInterface* browser_window = tracked_window->second;
      tracked_browser_windows_.erase(tracked_window);
      window_close_subscriptions_.erase(browser_window);
    }
    return {window, false};
  }

  auto tracked_window = tracked_browser_windows_.find(window_id);
  if (tracked_window != tracked_browser_windows_.end()) {
    return {tracked_window->second->GetWindow(), true};
  }
  return {nullptr, false};
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
    return protocol::Browser::WindowStateEnum::Fullscreen;
  }
  if (is_maximized) {
    return protocol::Browser::WindowStateEnum::Maximized;
  }
  if (is_minimized) {
    return protocol::Browser::WindowStateEnum::Minimized;
  }
  return protocol::Browser::WindowStateEnum::Normal;
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
  auto [window, is_pending] = FindWindowById(window_id);
  if (!window) {
    return Response::ServerError("Browser window not found");
  }

  gfx::Rect bounds = window->GetBounds();
  const bool set_bounds = window_bounds->HasLeft() || window_bounds->HasTop() ||
                          window_bounds->HasWidth() ||
                          window_bounds->HasHeight();
  if (set_bounds) {
    bounds.set_x(window_bounds->GetLeft(bounds.x()));
    bounds.set_y(window_bounds->GetTop(bounds.y()));
    bounds.set_width(window_bounds->GetWidth(bounds.width()));
    bounds.set_height(window_bounds->GetHeight(bounds.height()));
  }

  const std::string window_state =
      window_bounds->GetWindowState(protocol::Browser::WindowStateEnum::Normal);
  if (set_bounds &&
      window_state != protocol::Browser::WindowStateEnum::Normal) {
    return Response::InvalidParams(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (window_state == protocol::Browser::WindowStateEnum::Fullscreen) {
    return Response::ServerError("Fullscreen not supported on Android");
  }
  const std::string current_state = ResolveWindowState(window).name;
  if (window_state == protocol::Browser::WindowStateEnum::Maximized) {
    if (current_state == protocol::Browser::WindowStateEnum::Maximized) {
      return Response::Success();
    }
    if (current_state == protocol::Browser::WindowStateEnum::Minimized ||
        current_state == protocol::Browser::WindowStateEnum::Fullscreen) {
      return Response::ServerError(
          "To maximize a minimized or fullscreen "
          "window, restore it to normal state first.");
    }
    Response can_change = CheckCanChangeWindow(window, is_pending);
    if (!can_change.IsSuccess()) {
      return can_change;
    }
    window->Maximize();
    return Response::Success();
  }
  if (window_state == protocol::Browser::WindowStateEnum::Minimized) {
    if (current_state == protocol::Browser::WindowStateEnum::Minimized) {
      return Response::Success();
    }
    if (current_state == protocol::Browser::WindowStateEnum::Fullscreen) {
      return Response::ServerError(
          "To minimize a fullscreen window, restore it to normal "
          "state first.");
    }
    Response can_change = CheckCanChangeWindow(window, is_pending);
    if (!can_change.IsSuccess()) {
      return can_change;
    }
    window->Minimize();
    return Response::Success();
  }
  if (window_state != protocol::Browser::WindowStateEnum::Normal) {
    return Response::InvalidParams("Invalid windowState: " + window_state);
  }

  if (current_state == protocol::Browser::WindowStateEnum::Fullscreen) {
    return Response::ServerError("Cannot exit fullscreen on Android");
  }
  if (set_bounds) {
    Response can_change = CheckCanChangeWindow(window, is_pending);
    if (!can_change.IsSuccess()) {
      return can_change;
    }
    // Android's task implementation handles restoring a maximized/minimized
    // freeform window as part of applying explicit bounds.
    window->SetBounds(bounds);
  } else if (current_state == protocol::Browser::WindowStateEnum::Minimized ||
             current_state == protocol::Browser::WindowStateEnum::Maximized) {
    Response can_change = CheckCanChangeWindow(window, is_pending);
    if (!can_change.IsSuccess()) {
      return can_change;
    }
    window->Restore();
  }
  return Response::Success();
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
