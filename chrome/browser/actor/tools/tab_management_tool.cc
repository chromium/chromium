// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool.h"

#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace actor {

TabManagementTool::TabManagementTool(
    int32_t window_id,
    const optimization_guide::proto::CreateTabAction& action)
    : window_id_(window_id), action_(action) {}

TabManagementTool::~TabManagementTool() = default;

void TabManagementTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void TabManagementTool::Invoke(InvokeCallback callback) {
  BrowserWindowInterface* browser_window_interface =
      BrowserWindowInterface::FromSessionID(
          SessionID::FromSerializedValue(window_id_));
  if (!browser_window_interface) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  // TODO(bokan): Is the foreground bit always set? If not, should this return
  // an error or default to what? For now we default to foreground.
  WindowOpenDisposition disposition =
      (!action_.has_foreground() || action_.foreground())
          ? WindowOpenDisposition::NEW_FOREGROUND_TAB
          : WindowOpenDisposition::NEW_BACKGROUND_TAB;

  // Open a blank tab.
  browser_window_interface->OpenGURL(GURL(url::kAboutBlankURL), disposition);

  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string TabManagementTool::DebugString() const {
  return absl::StrFormat("TabManagementTool:%s", JournalEvent().c_str());
}

std::string TabManagementTool::JournalEvent() const {
  return "CreateTab";
}

bool TabManagementTool::RequiresFrame() const {
  // This is to avoid the kFrameWentAway check in
  // ToolController::ValidationComplete.
  return false;
}

}  // namespace actor
