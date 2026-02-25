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
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kUnableToFindTabError[] = "Unable to find tab.";
constexpr char kCannotMoveGroupIntoMiddleOfOtherGroupError[] =
    "Cannot move the group to an index that is in the middle of another group.";
constexpr char kCannotMoveGroupIntoMiddleOfPinnedTabsError[] =
    "Cannot move the group to an index that is in the middle of pinned tabs.";

// Returns true if a group could be moved into the |target_index| of the given
// |tab_strip|. Sets the |error| string otherwise.
bool IndexSupportsGroupMove(TabListInterface* tab_list,
                            int target_index,
                            std::string* error) {
  // A group can always be moved to the end of the tabstrip.
  if (target_index >= tab_list->GetTabCount() || target_index < 0) {
    return true;
  }

  tabs::TabInterface* target_tab = tab_list->GetTab(target_index);
  if (!target_tab) {
    *error = kUnableToFindTabError;
    return false;
  }

  if (target_tab->IsPinned()) {
    *error = kCannotMoveGroupIntoMiddleOfPinnedTabsError;
    return false;
  }

  std::optional<tab_groups::TabGroupId> target_group = target_tab->GetGroup();

  // Get the group to the left of the target, if there is one.
  std::optional<tab_groups::TabGroupId> adjacent_group;
  if (target_index > 0) {
    tabs::TabInterface* adjacent_tab = tab_list->GetTab(target_index - 1);
    CHECK(adjacent_tab);
    adjacent_group = adjacent_tab->GetGroup();
  }
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
  DCHECK(params.has_value());

  EXTENSION_FUNCTION_VALIDATE(params);
  int group_id = params->group_id;

  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  tab_groups::TabGroupVisualData visual_data;
  std::string error;
  if (!ExtensionTabUtil::GetGroupById(group_id, browser_context(),
                                      include_incognito_information(), nullptr,
                                      &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(!id.is_empty());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      ExtensionTabUtil::CreateTabGroupObject(id, visual_data))));
}

ExtensionFunction::ResponseAction TabGroupsQueryFunction::Run() {
  std::optional<api::tab_groups::Query::Params> params =
      api::tab_groups::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::ListValue result_list;
  Profile* profile = Profile::FromBrowserContext(browser_context());

  WindowController* window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (!window_controller) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }
  BrowserWindowInterface* current_browser =
      window_controller->GetBrowserWindowInterface();
  if (!current_browser) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window_interface) {
        if (!profile->IsSameOrParent(browser_window_interface->GetProfile())) {
          return true;
        }

        if (!include_incognito_information() &&
            profile != browser_window_interface->GetProfile()) {
          return true;
        }

        if (!BrowserExtensionWindowController::From(browser_window_interface)
                 ->IsVisibleToTabsAPIForExtension(
                     extension(), /*allow_dev_tools_windows=*/false)) {
          return true;
        }

        if (params->query_info.window_id) {
          const int window_id = *params->query_info.window_id;
          if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(
                                                 browser_window_interface)) {
            return true;
          }

          if (window_id == extension_misc::kCurrentWindowId &&
              browser_window_interface != current_browser) {
            return true;
          }
        }

        if (!ExtensionTabUtil::SupportsTabGroups(browser_window_interface)) {
          return true;
        }

        TabListInterface* tab_list =
            TabListInterface::From(browser_window_interface);
        if (!tab_list) {
          return true;
        }

        for (const tab_groups::TabGroupId& id : tab_list->ListTabGroups()) {
          std::optional<tab_groups::TabGroupVisualData> visual_data =
              tab_list->GetTabGroupVisualData(id);
          if (!visual_data) {
            continue;
          }

          if (params->query_info.collapsed &&
              *params->query_info.collapsed != visual_data->is_collapsed()) {
            continue;
          }

          if (params->query_info.title &&
              !base::MatchPattern(
                  visual_data->title(),
                  base::UTF8ToUTF16(*params->query_info.title))) {
            continue;
          }

          if (params->query_info.color != api::tab_groups::Color::kNone &&
              params->query_info.color !=
                  ExtensionTabUtil::ColorIdToColor(visual_data->color())) {
            continue;
          }

          if (params->query_info.shared.has_value() &&
              ExtensionTabUtil::GetSharedStateOfGroup(id) !=
                  params->query_info.shared.value()) {
            continue;
          }

          result_list.Append(
              ExtensionTabUtil::CreateTabGroupObject(id, *visual_data)
                  .ToValue());
        }
        return true;
      });

  return RespondNow(WithArguments(std::move(result_list)));
}

ExtensionFunction::ResponseAction TabGroupsUpdateFunction::Run() {
  std::optional<api::tab_groups::Update::Params> params =
      api::tab_groups::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int group_id = params->group_id;
  WindowController* window = nullptr;
  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  tab_groups::TabGroupVisualData visual_data;
  std::string error;
  if (!ExtensionTabUtil::GetGroupById(group_id, browser_context(),
                                      include_incognito_information(), &window,
                                      &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Since this is in a tab group, there should not be a prerender tab (with no
  // window).
  CHECK(window);

  DCHECK(!id.is_empty());

  bool collapsed = visual_data.is_collapsed();
  if (params->update_properties.collapsed) {
    collapsed = *params->update_properties.collapsed;
  }

  tab_groups::TabGroupColorId color = visual_data.color();
  if (params->update_properties.color != api::tab_groups::Color::kNone) {
    color = ExtensionTabUtil::ColorToColorId(params->update_properties.color);
  }

  std::u16string title = visual_data.title();
  if (params->update_properties.title) {
    title = base::UTF8ToUTF16(*params->update_properties.title);
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();
  if (!browser) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }

  if (!ExtensionTabUtil::SupportsTabGroups(browser)) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }

  // Update the visual data.
  auto* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }
  tab_groups::TabGroupVisualData new_visual_data(title, color, collapsed);
  tab_list->SetTabGroupVisualData(id, new_visual_data);

  if (!has_callback()) {
    return RespondNow(NoArguments());
  }

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      ExtensionTabUtil::CreateTabGroupObject(id, new_visual_data))));
}

////////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(IS_ANDROID)
// Helper class to observe for tab group creation notifications. Used on Android
// because cross-window tab group moves are asynchronous.
class TabGroupsMoveFunction::ObserverHelper : public TabModelObserver {
 public:
  ObserverHelper(TabGroupsMoveFunction* owner, TabModel* tab_model)
      : owner_(owner), tab_model_(tab_model) {
    tab_model_->AddObserver(this);
  }

  ~ObserverHelper() override { tab_model_->RemoveObserver(this); }

  // TabModelObserver:
  void OnTabGroupCreated(tab_groups::TabGroupId group_id) override {
    owner_->OnTabGroupCreated(group_id);
  }

 private:
  raw_ptr<TabGroupsMoveFunction> owner_;
  raw_ptr<TabModel> tab_model_;
};
#endif  // BUILDFLAG(IS_ANDROID)

TabGroupsMoveFunction::TabGroupsMoveFunction() = default;

TabGroupsMoveFunction::~TabGroupsMoveFunction() = default;

ExtensionFunction::ResponseAction TabGroupsMoveFunction::Run() {
  std::optional<api::tab_groups::Move::Params> params =
      api::tab_groups::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int group_id = params->group_id;
  int new_index = params->move_properties.index;
  const auto& window_id = params->move_properties.window_id;

  tab_groups::TabGroupId group = tab_groups::TabGroupId::CreateEmpty();
  bool cross_window = false;
  std::string error;
  const bool group_moved =
      MoveGroup(group_id, new_index, window_id, &group, &cross_window, &error);
  if (!group_moved) {
    return RespondNow(Error(std::move(error)));
  }

#if BUILDFLAG(IS_ANDROID)
  if (cross_window) {
    // Cross window group moves are asynchronous on Android. OnTabGroupCreated()
    // will be called later when the group is created in the new window.
    return RespondLater();
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (!has_callback()) {
    return RespondNow(NoArguments());
  }

  auto group_object = ExtensionTabUtil::CreateTabGroupObject(group);
  CHECK(group_object);
  return RespondNow(
      ArgumentList(api::tab_groups::Get::Results::Create(*group_object)));
}

bool TabGroupsMoveFunction::MoveGroup(int group_id,
                                      int new_index,
                                      const std::optional<int>& window_id,
                                      tab_groups::TabGroupId* group,
                                      bool* cross_window,
                                      std::string* error) {
  WindowController* source_window = nullptr;
  tab_groups::TabGroupVisualData visual_data;
  if (!ExtensionTabUtil::GetGroupById(
          group_id, browser_context(), include_incognito_information(),
          &source_window, group, &visual_data, error)) {
    return false;
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  auto* source_browser = source_window->GetBrowserWindowInterface();
  if (!source_browser) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  if (!ExtensionTabUtil::SupportsTabGroups(source_browser)) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  TabListInterface* source_tab_list = TabListInterface::From(source_browser);
  if (!source_tab_list) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  gfx::Range tabs = source_tab_list->GetTabGroupTabIndices(*group);
  if (tabs.length() == 0) {
    return false;
  }

  if (window_id) {
    WindowController* target_window = nullptr;
    if (!windows_util::GetControllerFromWindowID(
            this, *window_id, WindowController::GetAllWindowFilter(),
            &target_window, error)) {
      return false;
    }
    BrowserWindowInterface* target_browser =
        target_window->GetBrowserWindowInterface();
    if (!target_browser) {
      *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
      return false;
    }

    // TODO(crbug.com/40638654): Rather than calling is_type_normal(), should
    // this call SupportsWindowFeature(Browser::kFeatureTabstrip)?
    if (target_browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
      *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
      return false;
    }

    if (target_window->profile() != source_window->profile()) {
      *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError;
      return false;
    }

    // If windowId is different from the current window, move between windows.
    if (target_browser != source_browser) {
      *cross_window = true;
#if BUILDFLAG(IS_ANDROID)
      // Observe for OnTabGroupCreated() notifications, because cross-window
      // moves are asynchronous on Android.
      TabModel* target_tab_model =
          TabModelList::FindTabModelWithWindowSessionId(
              target_browser->GetSessionID());
      if (!target_tab_model) {
        *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
        return false;
      }
      observer_helper_ =
          std::make_unique<ObserverHelper>(this, target_tab_model);
#endif  // BUILDFLAG(IS_ANDROID)
      return MoveTabGroupBetweenBrowsers(source_browser, target_browser, *group,
                                         visual_data, tabs, new_index, error);
    }
  }

  // Perform a move within the same window.

  // When moving to the right, adjust the target index for the size of the
  // group, since the group itself may occupy several indices to the right.
  const int start_index = tabs.start();
  const int new_index_before_group_is_removed =
      new_index > start_index ? new_index + tabs.length() : new_index;

  if (!IndexSupportsGroupMove(source_tab_list,
                              new_index_before_group_is_removed, error)) {
    return false;
  }

  // Unlike when moving between windows, the index should be clamped to
  // count() - (#num of tabs in group being moved). Since the current tab(s)
  // being moved are within the same tabstrip, they can't be added beyond the
  // end of the occupied indices, but rather just shifted among them.
  const int size_after_group_removed =
      source_tab_list->GetTabCount() - tabs.length();
  if (new_index >= size_after_group_removed || new_index < 0) {
    new_index = size_after_group_removed;
  }

  if (new_index == start_index) {
    return true;
  }

  source_tab_list->MoveGroupTo(*group, new_index);

  return true;
}

bool TabGroupsMoveFunction::MoveTabGroupBetweenBrowsers(
    BrowserWindowInterface* source_browser,
    BrowserWindowInterface* target_browser,
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data,
    const gfx::Range& tabs,
    int new_index,
    std::string* error) {
  TabListInterface* target_tab_list =
      ExtensionTabUtil::GetEditableTabList(*target_browser);
  if (!target_tab_list) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  if (!ExtensionTabUtil::SupportsTabGroups(target_browser)) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  if (new_index > target_tab_list->GetTabCount() || new_index < 0) {
    new_index = target_tab_list->GetTabCount();
  }

  if (!IndexSupportsGroupMove(target_tab_list, new_index, error)) {
    return false;
  }

  TabListInterface* source_tab_list = TabListInterface::From(source_browser);
  if (!source_tab_list) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  // Pausing Saved Tab Groups is handled in TabListBridge on Win/Mac/Linux and
  // in MultiInstanceManagerApi31 on Android.
  source_tab_list->MoveTabGroupToWindow(group, target_browser->GetSessionID(),
                                        new_index);

  return true;
}

#if BUILDFLAG(IS_ANDROID)
void TabGroupsMoveFunction::OnTabGroupCreated(tab_groups::TabGroupId group_id) {
  observer_helper_.reset();

  auto group_object = ExtensionTabUtil::CreateTabGroupObject(group_id);
  CHECK(group_object);
  Respond(ArgumentList(api::tab_groups::Get::Results::Create(*group_object)));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace extensions
