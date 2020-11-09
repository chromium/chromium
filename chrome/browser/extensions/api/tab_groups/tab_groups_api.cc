// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace extensions {

ExtensionFunction::ResponseAction TabGroupsGetFunction::Run() {
  std::unique_ptr<api::tab_groups::Get::Params> params(
      api::tab_groups::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  int group_id = params->group_id;

  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  const tab_groups::TabGroupVisualData* visual_data = nullptr;
  std::string error;
  if (!tab_groups_util::GetGroupById(group_id, browser_context(),
                                     include_incognito_information(), nullptr,
                                     &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(!id.is_empty());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      *tab_groups_util::CreateTabGroupObject(id, *visual_data))));
}

ExtensionFunction::ResponseAction TabGroupsQueryFunction::Run() {
  std::unique_ptr<api::tab_groups::Query::Params> params(
      api::tab_groups::Query::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::Value result_list(base::Value::Type::LIST);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* current_browser =
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  for (auto* browser : *BrowserList::GetInstance()) {
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

    if (params->query_info.window_id.get()) {
      int window_id = *params->query_info.window_id;
      if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser))
        continue;

      if (window_id == extension_misc::kCurrentWindowId &&
          browser != current_browser) {
        continue;
      }
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    for (const tab_groups::TabGroupId& id :
         tab_strip->group_model()->ListTabGroups()) {
      const tab_groups::TabGroupVisualData* visual_data =
          tab_strip->group_model()->GetTabGroup(id)->visual_data();

      if (params->query_info.collapsed.get() &&
          *params->query_info.collapsed != visual_data->is_collapsed()) {
        continue;
      }

      if (params->query_info.title.get() &&
          !base::MatchPattern(visual_data->title(),
                              base::UTF8ToUTF16(*params->query_info.title))) {
        continue;
      }

      if (params->query_info.color != api::tab_groups::COLOR_NONE &&
          params->query_info.color !=
              tab_groups_util::ColorIdToColor(visual_data->color())) {
        continue;
      }

      result_list.Append(base::Value::FromUniquePtrValue(
          tab_groups_util::CreateTabGroupObject(id, *visual_data)->ToValue()));
    }
  }

  return RespondNow(OneArgument(std::move(result_list)));
}

ExtensionFunction::ResponseAction TabGroupsUpdateFunction::Run() {
  std::unique_ptr<api::tab_groups::Update::Params> params(
      api::tab_groups::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int group_id = params->group_id;
  Browser* browser = nullptr;
  tab_groups::TabGroupId id = tab_groups::TabGroupId::CreateEmpty();
  const tab_groups::TabGroupVisualData* visual_data = nullptr;
  std::string error;
  if (!tab_groups_util::GetGroupById(group_id, browser_context(),
                                     include_incognito_information(), &browser,
                                     &id, &visual_data, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  DCHECK(!id.is_empty());

  bool collapsed = visual_data->is_collapsed();
  if (params->update_properties.collapsed.get())
    collapsed = *params->update_properties.collapsed;

  tab_groups::TabGroupColorId color = visual_data->color();
  if (params->update_properties.color != api::tab_groups::COLOR_NONE)
    color = tab_groups_util::ColorToColorId(params->update_properties.color);

  base::string16 title = visual_data->title();
  if (params->update_properties.title.get())
    title = base::UTF8ToUTF16(*params->update_properties.title);

  TabGroup* tab_group =
      browser->tab_strip_model()->group_model()->GetTabGroup(id);

  tab_groups::TabGroupVisualData new_visual_data(title, color, collapsed);
  tab_group->SetVisualData(std::move(new_visual_data));

  if (!has_callback())
    return RespondNow(NoArguments());

  return RespondNow(ArgumentList(api::tab_groups::Get::Results::Create(
      *tab_groups_util::CreateTabGroupObject(tab_group->id(),
                                             *tab_group->visual_data()))));
}

}  // namespace extensions
