// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/switch_tab_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/tools/page_search_utils.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

SwitchTabTool::SwitchTabTool(TaskId task_id,
                             ToolDelegate& tool_delegate,
                             SessionID window_id,
                             std::string query)
    : Tool(task_id, tool_delegate),
      window_id_(window_id),
      query_(std::move(query)) {}

SwitchTabTool::~SwitchTabTool() = default;

void SwitchTabTool::Validate(ToolCallback callback) {
  BrowserWindowInterface* browser = GetBrowser();
  mojom::ActionResultPtr window_result = ValidateBrowserWindow(browser);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback), std::move(window_result));
    return;
  }

  std::vector<TabMatch> matches = FindMatchingTabs(browser, query_);

  // Ignore the already-active tab: switching to the tab the user is currently
  // viewing would be a no-op, and keeping it in `matches` would cause a false
  // ambiguity error whenever both the active tab and a single background tab
  // match `query_`.
  const size_t erased_active_tabs =
      std::erase_if(matches, [](const TabMatch& match) {
        tabs::TabInterface* tab = match.handle.Get();
        return !tab || tab->IsActivated();
      });

  if (matches.empty()) {
    if (erased_active_tabs > 0) {
      PostResponseTask(
          std::move(callback),
          MakeResult(mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab,
                     /*requires_page_stabilization=*/false,
                     absl::StrFormat("The active tab already matches \"%s\".",
                                     query_)));
      return;
    }
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kPageSearchNoMatch,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("No open tab matched \"%s\".", query_)));
    return;
  }

  if (matches.size() > 1) {
    // TODO(b/565736354): Once tools support returning structured values for
    // TTC, return a JSON payload of candidate tabs (with tab ID, title, and
    // URL) and consider returning ActionResultCode::kOk instead of an actor
    // error (pending security review for prompt injection risk).
    // Ambiguity across multiple background tabs is an error rather than a
    // guess: the caller is the only one that can say which tab it meant, so
    // hand back the candidates and let it retry with a more specific query.
    std::vector<std::string> titles;
    titles.reserve(matches.size());
    for (const TabMatch& match : matches) {
      titles.push_back(base::UTF16ToUTF8(match.title));
    }
    PostResponseTask(
        std::move(callback),
        MakeResult(
            mojom::ActionResultCode::kPageSearchAmbiguousMatch,
            /*requires_page_stabilization=*/false,
            absl::StrFormat(
                "%d open tabs matched \"%s\": %s. Retry with a query "
                "that matches only one of them.",
                matches.size(), query_, base::JoinString(titles, ", "))));
    return;
  }

  match_ = matches.front();

  ValidateUrlIsAcceptableNavigationDestination(match_->url, tool_delegate(),
                                               std::move(callback));
}

void SwitchTabTool::Invoke(ToolCallback callback) {
  CHECK(match_.has_value());

  // The tab may have been closed, or dragged into a different window, between
  // Validate() and now. ResolveTabInWindow() rejects both cases; acting on the
  // bare handle would let a tab switch pull focus to another window.
  BrowserWindowInterface* browser = GetBrowser();
  tabs::TabInterface* tab =
      browser ? ResolveTabInWindow(*match_, browser) : nullptr;
  if (!tab) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kPageSearchNoMatch,
                   /*requires_page_stabilization=*/false,
                   "The matched tab is no longer open in this window."));
    return;
  }

  // TODO(b/565736154): Replace this delegation once composite tools are
  // supported (b/555808355).
  activate_tool_ = std::make_unique<TabManagementTool>(
      task_id(), tool_delegate(), TabManagementTool::kActivate, match_->handle);
  activate_tool_->Invoke(std::move(callback));
}

std::string SwitchTabTool::DebugString() const {
  return absl::StrFormat("SwitchTabTool:query(%s)", query_);
}

std::string SwitchTabTool::JournalEvent() const {
  return "SwitchTab";
}

std::unique_ptr<ObservationDelayController>
SwitchTabTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // Activating an already-loaded tab doesn't navigate, so there is nothing to
  // wait on.
  return nullptr;
}

tabs::TabHandle SwitchTabTool::GetTargetTab() const {
  // Null until Validate() resolves the query.
  return match_.has_value() ? match_->handle : tabs::TabHandle::Null();
}

BrowserWindowInterface* SwitchTabTool::GetBrowser() const {
  return BrowserWindowInterface::FromSessionID(window_id_);
}

}  // namespace actor
