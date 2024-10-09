// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"

using tabs::TabModel;

namespace extensions {

namespace {

constexpr char kCannotMoveGroupIntoMiddleOfOtherGroupError[] =
    "Cannot move the group to an index that is in the middle of another group.";
constexpr char kCannotMoveGroupIntoMiddleOfPinnedTabsError[] =
    "Cannot move the group to an index that is in the middle of pinned tabs.";

// Returns true if a group could be moved into the |target_index| of the given
// |tab_strip|. Sets the |error| string otherwise.
bool IndexSupportsGroupMove(TabStripModel* tab_strip,
                            int target_index,
                            std::string* error) {
  // A group can always be moved to the end of the tabstrip.
  if (target_index >= tab_strip->count() || target_index < 0) {
    return true;
  }

  if (tab_strip->IsTabPinned(target_index)) {
    *error = kCannotMoveGroupIntoMiddleOfPinnedTabsError;
    return false;
  }

  std::optional<tab_groups::TabGroupId> target_group =
      tab_strip->GetTabGroupForTab(target_index);
  std::optional<tab_groups::TabGroupId> adjacent_group =
      tab_strip->GetTabGroupForTab(target_index - 1);

  if (target_group.has_value() && target_group == adjacent_group) {
    *error = kCannotMoveGroupIntoMiddleOfOtherGroupError;
    return false;
  }

  return true;
}

}  // namespace

ExtensionFunction::ResponseAction TabGroupsGetFunction::Run() {
  std::optional<api::tab_groups::Get::Params> params =
      api::tab_groups::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int group_id = params->group_id;

  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  const tab_groups::TabGroupVisualData* visual_data = nullptr;
  std::string error;
  if (!ExtensionTabUtil::GetGroupById(group_id, browser_context(),
                                      include_incognito_information(), nullptr,
                                      &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(!id.is_empty());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      ExtensionTabUtil::CreateTabGroupObject(id, *visual_data))));
}

ExtensionFunction::ResponseAction TabGroupsQueryFunction::Run() {
  std::optional<api::tab_groups::Query::Params> params =
      api::tab_groups::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::Value::List result_list;
  Profile* profile = Profile::FromBrowserContext(browser_context());

  WindowController* window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (!window_controller) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }
  Browser* current_browser = window_controller->GetBrowser();

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!profile->IsSameOrParent(browser->profile()))
      continue;

    if (!browser->window())
      continue;

    if (!include_incognito_information() && profile != browser->profile())
      continue;

    if (!browser->extension_window_controller()->IsVisibleToTabsAPIForExtension(
            extension(), false /*allow_dev_tools_windows*/)) {
      continue;
    }

    if (params->query_info.window_id) {
      int window_id = *params->query_info.window_id;
      if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser))
        continue;

      if (window_id == extension_misc::kCurrentWindowId &&
          browser != current_browser) {
        continue;
      }
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip->SupportsTabGroups()) {
      continue;
    }

    for (const tab_groups::TabGroupId& id :
         tab_strip->group_model()->ListTabGroups()) {
      const tab_groups::TabGroupVisualData* visual_data =
          tab_strip->group_model()->GetTabGroup(id)->visual_data();

      if (params->query_info.collapsed &&
          *params->query_info.collapsed != visual_data->is_collapsed()) {
        continue;
      }

      if (params->query_info.title &&
          !base::MatchPattern(visual_data->title(),
                              base::UTF8ToUTF16(*params->query_info.title))) {
        continue;
      }

      if (params->query_info.color != api::tab_groups::Color::kNone &&
          params->query_info.color !=
              ExtensionTabUtil::ColorIdToColor(visual_data->color())) {
        continue;
      }

      result_list.Append(
          ExtensionTabUtil::CreateTabGroupObject(id, *visual_data).ToValue());
    }
  }

  return RespondNow(WithArguments(std::move(result_list)));
}

ExtensionFunction::ResponseAction TabGroupsUpdateFunction::Run() {
  std::optional<api::tab_groups::Update::Params> params =
      api::tab_groups::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int group_id = params->group_id;
  Browser* browser = nullptr;
  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  const tab_groups::TabGroupVisualData* visual_data = nullptr;
  std::string error;
  if (!ExtensionTabUtil::GetGroupById(group_id, browser_context(),
                                      include_incognito_information(), &browser,
                                      &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(!id.is_empty());

  bool collapsed = visual_data->is_collapsed();
  if (params->update_properties.collapsed)
    collapsed = *params->update_properties.collapsed;

  tab_groups::TabGroupColorId color = visual_data->color();
  if (params->update_properties.color != api::tab_groups::Color::kNone) {
    color = ExtensionTabUtil::ColorToColorId(params->update_properties.color);
  }

  std::u16string title = visual_data->title();
  if (params->update_properties.title)
    title = base::UTF8ToUTF16(*params->update_properties.title);

  TabStripModel* tab_strip_model =
      ExtensionTabUtil::GetEditableTabStripModel(browser);
  if (!tab_strip_model)
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  if (!tab_strip_model->SupportsTabGroups())
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(id);

  tab_groups::TabGroupVisualData new_visual_data(title, color, collapsed);
  tab_group->SetVisualData(std::move(new_visual_data));

  if (!has_callback())
    return RespondNow(NoArguments());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      ExtensionTabUtil::CreateTabGroupObject(tab_group->id(),
                                             *tab_group->visual_data()))));
}

ExtensionFunction::ResponseAction TabGroupsMoveFunction::Run() {
  std::optional<api::tab_groups::Move::Params> params =
      api::tab_groups::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int group_id = params->group_id;
  int new_index = params->move_properties.index;
  const auto& window_id = params->move_properties.window_id;

  tab_groups::TabGroupId group = tab_groups::TabGroupId::CreateEmpty();
  std::string error;
  const bool group_moved =
      MoveGroup(group_id, new_index, window_id, &group, &error);
  if (!group_moved) {
    return RespondNow(Error(std::move(error)));
  }

  if (!has_callback())
    return RespondNow(NoArguments());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      *ExtensionTabUtil::CreateTabGroupObject(group))));
}

bool TabGroupsMoveFunction::MoveGroup(int group_id,
                                      int new_index,
                                      const std::optional<int>& window_id,
                                      tab_groups::TabGroupId* group,
                                      std::string* error) {
  Browser* source_browser = nullptr;
  const tab_groups::TabGroupVisualData* visual_data = nullptr;
  if (!ExtensionTabUtil::GetGroupById(
          group_id, browser_context(), include_incognito_information(),
          &source_browser, group, &visual_data, error)) {
    return false;
  }

  TabStripModel* source_tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(source_browser);
  if (!source_tab_strip) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  if (!source_tab_strip->SupportsTabGroups()) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  gfx::Range tabs =
      source_tab_strip->group_model()->GetTabGroup(*group)->ListTabs();
  if (tabs.length() == 0) {
    return false;
  }

  if (window_id) {
    WindowController* window_controller = nullptr;
    if (!windows_util::GetControllerFromWindowID(
            this, *window_id, WindowController::GetAllWindowFilter(),
            &window_controller, error)) {
      return false;
    }
    Browser* target_browser = window_controller->GetBrowser();

    // TODO(crbug.com/40638654): Rather than calling is_type_normal(), should
    // this call SupportsWindowFeature(Browser::FEATURE_TABSTRIP)?
    if (!target_browser->is_type_normal()) {
      *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
      return false;
    }

    if (target_browser->profile() != source_browser->profile()) {
      *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError;
      return false;
    }

    // If windowId is different from the current window, move between windows.
    if (target_browser == source_browser) {
      return false;
    }

    TabStripModel* target_tab_strip =
        ExtensionTabUtil::GetEditableTabStripModel(target_browser);
    if (!target_tab_strip) {
      *error = ExtensionTabUtil::kTabStripNotEditableError;
      return false;
    }

    if (!target_tab_strip->SupportsTabGroups()) {
      *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
      return false;
    }

    if (new_index > target_tab_strip->count() || new_index < 0) {
      new_index = target_tab_strip->count();
    }

    if (!IndexSupportsGroupMove(target_tab_strip, new_index, error)) {
      return false;
    }

    target_tab_strip->group_model()->AddTabGroup(*group, *visual_data);

    for (size_t i = 0; i < tabs.length(); ++i) {
      // Detach tabs from the same index each time, since each detached tab is
      // removed from the model, and groups are always contiguous.
      std::unique_ptr<TabModel> detached_tab =
          source_tab_strip->DetachTabAtForInsertion(tabs.start());

      // Attach tabs in consecutive indices, to insert them in the same order.
      target_tab_strip->InsertDetachedTabAt(new_index + i,
                                            std::move(detached_tab),
                                            AddTabTypes::ADD_NONE, *group);
    }

    return true;
  }

  // Perform a move within the same window.

  // When moving to the right, adjust the target index for the size of the
  // group, since the group itself may occupy several indices to the right.
  const int start_index = tabs.start();
  const int new_index_before_group_is_removed =
      new_index > start_index ? new_index + tabs.length() : new_index;

  if (!IndexSupportsGroupMove(source_tab_strip,
                              new_index_before_group_is_removed, error)) {
    return false;
  }

  // Unlike when moving between windows, the index should be clamped to
  // count() - (#num of tabs in group being moved). Since the current tab(s)
  // being moved are within the same tabstrip, they can't be added beyond the
  // end of the occupied indices, but rather just shifted among them.
  const int size_after_group_removed =
      source_tab_strip->count() - tabs.length();
  if (new_index >= size_after_group_removed || new_index < 0) {
    new_index = size_after_group_removed;
  }

  if (new_index == start_index) {
    return true;
  }

  source_tab_strip->MoveGroupTo(*group, new_index);

  return true;
}

}  // namespace extensions
