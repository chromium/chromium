// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_util.h"

#include <optional>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/browser/api/constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/error_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#endif

namespace extensions::tabs_util {

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr char kCannotMoveIwaTabError[] =
    "The tab of an Isolated Web App cannot be moved.";
#endif

// Returns the tab group ID for the tab at `index`. Returns nullopt if the index
// is out of range, the tab is not found, or the tab is not part of a group.
std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
    TabListInterface& tab_list,
    int index) {
  if (index < 0 || index >= tab_list.GetTabCount()) {
    return std::nullopt;
  }
  ::tabs::TabInterface* tab = tab_list.GetTab(index);
  CHECK(tab);
  return tab->GetGroup();
}

}  // namespace

bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                WindowController** window_out,
                content::WebContents** contents_out,
                int* index_out,
                std::string* error_out) {
  if (ExtensionTabUtil::GetTabById(tab_id, context, include_incognito,
                                   window_out, contents_out, index_out)) {
    return true;
  }

  if (error_out) {
    *error_out = ErrorUtils::FormatErrorMessage(kTabNotFoundError,
                                                base::NumberToString(tab_id));
  }

  return false;
}

base::expected<void, std::string> ValidateTabForMove(
    WindowController* source_window,
    Profile* window_profile,
    content::WebContents* web_contents,
    bool is_locked_fullscreen) {
  if (!source_window) {
    // The source window can be null for prerender tabs.
    return base::unexpected(tabs_constants::kInvalidWindowStateError);
  }
  if (!source_window->GetBrowserWindowInterface()) {
    return base::unexpected(
        ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError);
  }
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* source_browser = source_window->GetBrowser();
  CHECK(source_browser);
  if (web_app::AppBrowserController* controller =
          web_app::AppBrowserController::From(source_browser);
      controller && controller->IsIsolatedWebApp()) {
    return base::unexpected(kCannotMoveIwaTabError);
  }
#endif

  if (!ExtensionTabUtil::IsTabStripEditable(*source_window->profile())) {
    return base::unexpected(ExtensionTabUtil::kTabStripNotEditableError);
  }

  if (source_window->profile() != window_profile) {
    return base::unexpected(
        ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError);
  }

  if (DevToolsWindow::IsDevToolsWindow(web_contents)) {
    return base::unexpected(tabs_constants::kNotAllowedForDevToolsError);
  }

  return {};
}

int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    BrowserWindowInterface* target_browser,
                    int new_index,
                    bool allow_other_window_types,
                    std::string* error) {
  WindowController* source_window = nullptr;
  content::WebContents* web_contents = nullptr;
  int source_index = -1;
  if (!GetTabById(tab_id, function->browser_context(),
                  function->include_incognito_information(), &source_window,
                  &web_contents, &source_index, error)) {
    return -1;
  }

  if (!target_browser) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  auto validation_result = ValidateTabForMove(
      source_window, target_browser->GetProfile(), web_contents);
  if (!validation_result.has_value()) {
    *error = std::move(validation_result.error());
    return -1;
  }

  // TODO(crbug.com/40638654): Rather than calling checking against
  // TYPE_NORMAL, should this call
  // SupportsWindowFeature(Browser::kFeatureTabstrip)?
  if (!allow_other_window_types &&
      target_browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  TabListInterface* target_tab_list =
      ExtensionTabUtil::GetEditableTabList(*target_browser);
  CHECK(target_tab_list);

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  int target_index = new_index;
  if (target_index > target_tab_list->GetTabCount() || target_index < 0) {
    target_index = target_tab_list->GetTabCount();
  }

  TabListInterface* tab_list =
      ExtensionTabUtil::GetEditableTabList(*target_browser);
  CHECK(tab_list);
  if (ExtensionTabUtil::SupportsTabGroups(target_browser)) {
    std::optional<tab_groups::TabGroupId> next_tab_dst_group =
        GetTabGroupForTab(*tab_list, target_index);

    std::optional<tab_groups::TabGroupId> prev_tab_dst_group =
        GetTabGroupForTab(*tab_list, target_index - 1);

    // Group contiguity is not respected in the target tabstrip.
    if (next_tab_dst_group.has_value() && prev_tab_dst_group.has_value() &&
        next_tab_dst_group == prev_tab_dst_group) {
      *error = tabs_constants::kInvalidTabIndexBreaksGroupContiguity;
      return -1;
    }
  }

  BrowserWindowInterface* source_browser =
      source_window->GetBrowserWindowInterface();
  CHECK(source_browser);

  TabListInterface* source_tab_list = TabListInterface::From(source_browser);
  ::tabs::TabInterface* tab = source_tab_list->GetTab(source_index);
  if (!tab) {
    *error = ErrorUtils::FormatErrorMessage(kTabNotFoundError,
                                            base::NumberToString(tab_id));
    return -1;
  }

  source_tab_list->MoveTabToWindow(
      tab->GetHandle(), target_browser->GetSessionID(), target_index);

  // The new index may differ from `target_index` if the target index was
  // invalid for any reason, or could be -1 if the move failed.
  int final_index = target_tab_list->GetIndexOfTab(tab->GetHandle());
  return final_index;
}

}  // namespace extensions::tabs_util
