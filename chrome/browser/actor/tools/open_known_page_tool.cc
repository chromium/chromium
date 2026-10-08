// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/open_known_page_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/page_search_utils.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

OpenKnownPageTool::OpenKnownPageTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     SessionID window_id,
                                     std::string query)
    : Tool(task_id, tool_delegate),
      window_id_(window_id),
      query_(std::move(query)) {}

OpenKnownPageTool::~OpenKnownPageTool() = default;

void OpenKnownPageTool::Validate(ToolCallback callback) {
  BrowserWindowInterface* browser = GetBrowser();
  mojom::ActionResultPtr window_result = ValidateBrowserWindow(browser);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback), std::move(window_result));
    return;
  }

  // Stage 1: Tabs (Synchronous).
  // TODO(b/565736154): Delegate Stage 1 directly to SwitchTabTool once
  // composite tools are supported (b/555808355).
  std::vector<TabMatch> tab_matches = FindMatchingTabs(browser, query_);
  size_t erased_active_tabs = 0;
  std::erase_if(tab_matches, [&](const TabMatch& match) {
    tabs::TabInterface* tab = match.handle.Get();
    if (!tab) {
      return true;
    }
    if (tab->IsActivated()) {
      ++erased_active_tabs;
      return true;
    }
    return false;
  });

  if (!tab_matches.empty()) {
    if (tab_matches.size() > 1) {
      // TODO(b/565736354): Once tools support returning structured values for
      // TTC, return a JSON payload of candidate tabs (with tab ID, title, and
      // URL) and consider returning ActionResultCode::kOk instead of an actor
      // error (pending security review for prompt injection risk).
      std::vector<std::string> titles;
      titles.reserve(tab_matches.size());
      for (const TabMatch& match : tab_matches) {
        titles.push_back(match.title.empty() ? match.url.spec()
                                             : base::UTF16ToUTF8(match.title));
      }
      PostResponseTask(
          std::move(callback),
          MakeResult(
              mojom::ActionResultCode::kPageSearchAmbiguousMatch,
              /*requires_page_stabilization=*/false,
              absl::StrFormat(
                  "%zu open tabs matched \"%s\": %s. Retry with a query "
                  "that matches only one of them.",
                  tab_matches.size(), query_, base::JoinString(titles, ", "))));
      return;
    }

    matched_tab_ = tab_matches.front();
    ValidateDestinationUrl(matched_tab_->url, std::move(callback));
    return;
  }

  if (erased_active_tabs > 0) {
    // The query matched the currently active tab and no background tabs. Return
    // early rather than falling through to History/Bookmarks, which would
    // redundantly re-navigate the tab the user is already viewing.
    PostResponseTask(
        std::move(callback),
        MakeResult(
            mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab,
            /*requires_page_stabilization=*/false,
            absl::StrFormat("The active tab already matches \"%s\".", query_)));
    return;
  }

  // Stage 2: History (Asynchronous).
  FindMatchingHistory(
      &tool_delegate().GetProfile(), query_, &cancelable_task_tracker_,
      base::BindOnce(&OpenKnownPageTool::OnHistoryMatches,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OpenKnownPageTool::OnHistoryMatches(
    ToolCallback callback,
    std::vector<PageMatch> history_matches) {
  BrowserWindowInterface* browser = GetBrowser();
  mojom::ActionResultPtr window_result = ValidateBrowserWindow(browser);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback), std::move(window_result));
    return;
  }

  std::optional<PageMatch> chosen_page;
  if (!history_matches.empty()) {
    // TODO(b/565736354): Once tools support returning structured values for
    // TTC, return the candidate list when multiple history or bookmark entries
    // match so the model can disambiguate, rather than always picking the top
    // match.
    chosen_page = history_matches.front();
  } else {
    // Stage 3: Bookmarks (Synchronous fallback).
    std::vector<PageMatch> bookmark_matches =
        FindMatchingBookmarks(&tool_delegate().GetProfile(), query_);
    if (!bookmark_matches.empty()) {
      chosen_page = bookmark_matches.front();
    }
  }

  if (!chosen_page.has_value()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kPageSearchNoMatch,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("No open tab, history entry, or bookmark "
                                   "matched \"%s\".",
                                   query_)));
    return;
  }

  TabStripModel* tab_strip_model = browser->GetTabStripModel();
  tabs::TabInterface* active_tab =
      tab_strip_model ? tab_strip_model->GetActiveTab() : nullptr;
  if (!active_tab || !active_tab->GetContents()) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway,
                                /*requires_page_stabilization=*/false,
                                "The active tab is no longer present."));
    return;
  }

  // Before navigating `active_tab` to `chosen_page->url`, check whether an
  // open tab in this window is already on an equivalent page (ignoring `#ref`
  // fragments). Return early before constructing `navigate_tool_` so
  // `GetObservationDelayer()` does not wait on a navigation that `Invoke()`
  // will not execute.
  //
  // If the active tab already matches, stay on it rather than switching to a
  // background tab or reloading.
  const GURL& active_url = active_tab->GetContents()->GetLastCommittedURL();
  if (IsAllowedMatchUrl(active_url) &&
      AreUrlsEquivalentForDeduplication(active_url, chosen_page->url)) {
    PostResponseTask(
        std::move(callback),
        MakeResult(
            mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab,
            /*requires_page_stabilization=*/false,
            absl::StrFormat("The active tab already matches \"%s\".", query_)));
    return;
  }

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(i);
    if (tab == active_tab) {
      continue;
    }
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    if (!contents) {
      continue;
    }
    const GURL& committed_url = contents->GetLastCommittedURL();
    if (!IsAllowedMatchUrl(committed_url) ||
        !AreUrlsEquivalentForDeduplication(committed_url, chosen_page->url)) {
      continue;
    }
    matched_tab_ =
        TabMatch{tab->GetHandle(), committed_url, contents->GetTitle()};
    ValidateDestinationUrl(matched_tab_->url, std::move(callback));
    return;
  }

  ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::From(active_tab->GetHandle());
  if (!actor_surface_handle.Get()) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway,
                                /*requires_page_stabilization=*/false,
                                "The active tab is no longer present."));
    return;
  }

  navigation_target_ =
      TabMatch{active_tab->GetHandle(), chosen_page->url, chosen_page->title};
  // TODO(b/565736154): Replace this delegation once composite tools are
  // supported (b/555808355).
  navigate_tool_ = std::make_unique<NavigateTool>(task_id(), tool_delegate(),
                                                  *actor_surface_handle.Get(),
                                                  chosen_page->url);

  ValidateDestinationUrl(chosen_page->url, std::move(callback));
}

void OpenKnownPageTool::ValidateDestinationUrl(const GURL& url,
                                               ToolCallback callback) {
  ValidateUrlIsAcceptableNavigationDestination(url, tool_delegate(),
                                               std::move(callback));
}

void OpenKnownPageTool::Invoke(ToolCallback callback) {
  BrowserWindowInterface* browser = GetBrowser();
  mojom::ActionResultPtr window_result = ValidateBrowserWindow(browser);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback), std::move(window_result));
    return;
  }

  if (matched_tab_.has_value()) {
    tabs::TabInterface* tab = ResolveTabInWindow(*matched_tab_, browser);
    if (!tab) {
      PostResponseTask(
          std::move(callback),
          MakeResult(mojom::ActionResultCode::kTabWentAway,
                     /*requires_page_stabilization=*/false,
                     "The matched tab is no longer open in this window."));
      return;
    }

    // TODO(b/565736154): Replace this delegation once composite tools are
    // supported (b/555808355).
    activate_tool_ = std::make_unique<TabManagementTool>(
        task_id(), tool_delegate(), TabManagementTool::kActivate,
        matched_tab_->handle);
    activate_tool_->Invoke(std::move(callback));
    return;
  }

  CHECK(navigation_target_.has_value());
  tabs::TabInterface* tab = ResolveTabInWindow(*navigation_target_, browser);
  ActorSurfaceHandle actor_surface_handle =
      tab ? ActorSurfaceHandle::From(tab->GetHandle())
          : ActorSurfaceHandle::Null();
  if (!actor_surface_handle.Get()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kTabWentAway,
                   /*requires_page_stabilization=*/false,
                   "The target tab is no longer open in this window."));
    return;
  }

  // Recreate NavigateTool to bind to the live WebContents, guarding against
  // WebContents replacement or discard between Validate() and Invoke().
  // TODO(b/565736154): Replace this delegation once composite tools are
  // supported (b/555808355).
  navigate_tool_ = std::make_unique<NavigateTool>(task_id(), tool_delegate(),
                                                  *actor_surface_handle.Get(),
                                                  navigation_target_->url);
  navigate_tool_->Invoke(std::move(callback));
}

std::string OpenKnownPageTool::DebugString() const {
  return absl::StrFormat("OpenKnownPageTool:query(%s)", query_);
}

std::string OpenKnownPageTool::JournalEvent() const {
  return "OpenKnownPage";
}

std::unique_ptr<ObservationDelayController>
OpenKnownPageTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (navigate_tool_ && navigate_tool_->GetTargetActorSurface().Get()) {
    return navigate_tool_->GetObservationDelayer(page_stability_config);
  }
  return nullptr;
}

void OpenKnownPageTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                               ToolCallback callback) const {
  if (navigate_tool_) {
    navigate_tool_->UpdateTaskBeforeInvoke(task, std::move(callback));
    return;
  }
  Tool::UpdateTaskBeforeInvoke(task, std::move(callback));
}

tabs::TabHandle OpenKnownPageTool::GetTargetTab() const {
  if (matched_tab_.has_value()) {
    return matched_tab_->handle;
  }
  if (navigation_target_.has_value()) {
    return navigation_target_->handle;
  }
  return tabs::TabHandle::Null();
}

BrowserWindowInterface* OpenKnownPageTool::GetBrowser() const {
  return BrowserWindowInterface::FromSessionID(window_id_);
}

}  // namespace actor
