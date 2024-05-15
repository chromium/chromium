// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler.h"

#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_dock_tile.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"

using protocol::Maybe;
using protocol::Response;

namespace {

BrowserWindow* GetBrowserWindow(int window_id) {
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b->session_id().id() == window_id)
      return b->window();
  }
  return nullptr;
}

std::unique_ptr<protocol::Browser::Bounds> GetBrowserWindowBounds(
    BrowserWindow* window) {
  std::string window_state = "normal";
  if (window->IsMinimized())
    window_state = "minimized";
  if (window->IsMaximized())
    window_state = "maximized";
  if (window->IsFullscreen())
    window_state = "fullscreen";

  gfx::Rect bounds;
  if (window->IsMinimized())
    bounds = window->GetRestoredBounds();
  else
    bounds = window->GetBounds();
  return protocol::Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(window_state)
      .Build();
}

}  // namespace

BrowserHandler::BrowserHandler(protocol::UberDispatcher* dispatcher,
                               const std::string& target_id)
    : target_id_(target_id) {
  // Dispatcher can be null in tests.
  if (dispatcher)
    protocol::Browser::Dispatcher::wire(dispatcher, this);
}

BrowserHandler::~BrowserHandler() = default;

Response BrowserHandler::GetWindowForTarget(
    protocol::Maybe<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  auto host =
      content::DevToolsAgentHost::GetForId(target_id.value_or(target_id_));
  if (!host)
    return Response::ServerError("No target with given id");
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents)
    return Response::ServerError("No web contents in the target");

  Browser* browser = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    int tab_index = b->tab_strip_model()->GetIndexOfWebContents(web_contents);
    if (tab_index != TabStripModel::kNoTab)
      browser = b;
  }
  if (!browser)
    return Response::ServerError("Browser window not found");

  BrowserWindow* window = browser->window();
  *out_window_id = browser->session_id().id();
  *out_bounds = GetBrowserWindowBounds(window);
  return Response::Success();
}

Response BrowserHandler::GetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return Response::ServerError("Browser window not found");

  *out_bounds = GetBrowserWindowBounds(window);
  return Response::Success();
}

Response BrowserHandler::Close() {
  ChromeDevToolsManagerDelegate::CloseBrowserSoon();
  return Response::Success();
}

Response BrowserHandler::SetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds> window_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return Response::ServerError("Browser window not found");
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

  const std::string window_state = window_bounds->GetWindowState("normal");
  if (set_bounds && window_state != "normal") {
    return Response::ServerError(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (window_state == "fullscreen") {
    if (window->IsMinimized()) {
      return Response::ServerError(
          "To make minimized window fullscreen, "
          "restore it to normal state first.");
    }
    window->GetExclusiveAccessContext()->EnterFullscreen(
        GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, display::kInvalidDisplayId);
  } else if (window_state == "maximized") {
    if (window->IsMinimized() || window->IsFullscreen()) {
      return Response::ServerError(
          "To maximize a minimized or fullscreen "
          "window, restore it to normal state first.");
    }
    window->Maximize();
  } else if (window_state == "minimized") {
    if (window->IsFullscreen()) {
      return Response::ServerError(
          "To minimize a fullscreen window, restore it to normal "
          "state first.");
    }
    window->Minimize();
  } else if (window_state == "normal") {
    if (window->IsFullscreen()) {
      window->GetExclusiveAccessContext()->ExitFullscreen();
    } else if (window->IsMinimized() || window->IsMaximized()) {
      window->Restore();
    } else if (set_bounds) {
      window->SetBounds(bounds);
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return Response::Success();
}

protocol::Response BrowserHandler::SetDockTile(
    protocol::Maybe<std::string> label,
    protocol::Maybe<protocol::Binary> image) {
  std::vector<gfx::ImagePNGRep> reps;
  if (image.has_value()) {
    reps.emplace_back(image.value().bytes(), 1);
  }
  DevToolsDockTile::Update(label.value_or(std::string()),
                           !reps.empty() ? gfx::Image(reps) : gfx::Image());
  return Response::Success();
}

protocol::Response BrowserHandler::ExecuteBrowserCommand(
    const protocol::Browser::BrowserCommandId& command_id) {
  static auto& command_id_map =
      *new std::map<protocol::Browser::BrowserCommandId, int>{
          {protocol::Browser::BrowserCommandIdEnum::OpenTabSearch,
           IDC_TAB_SEARCH},
          {protocol::Browser::BrowserCommandIdEnum::CloseTabSearch,
           IDC_TAB_SEARCH_CLOSE}};
  if (command_id_map.count(command_id) == 0) {
    return Response::InvalidParams("Invalid BrowserCommandId: " + command_id);
  }
  if (!chrome::ExecuteCommand(BrowserList::GetInstance()->GetLastActive(),
                              command_id_map[command_id])) {
    return Response::InvalidRequest(
        "Browser command not supported. BrowserCommandId: " + command_id);
  }
  return Response::Success();
}

protocol::Response BrowserHandler::AddPrivacySandboxEnrollmentOverride(
    const std::string& in_url) {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (!host) {
    return Response::ServerError("No host found");
  }

  GURL url_to_add = GURL(in_url);

  if (!url_to_add.is_valid()) {
    return Response::InvalidParams("Invalid URL");
  }

  privacy_sandbox::PrivacySandboxAttestations::GetInstance()->AddOverride(
      net::SchemefulSite(url_to_add));
  return Response::Success();
}
