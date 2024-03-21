// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/session_types.h"

namespace full_restore {

namespace {

constexpr size_t kMaxUrls = 5u;

}  // namespace

crosapi::mojom::SessionWindowPtr ToSessionWindowPtr(
    sessions::SessionWindow* session_window) {
  crosapi::mojom::SessionWindowPtr new_session_window =
      crosapi::mojom::SessionWindow::New();
  new_session_window->window_id = session_window->window_id.id();

  // App browsers app ID is the same as regular chrome browsers. To get the
  // correct icon and title from the app service, we need to find the app
  // name and remove the "_crx_", then use that result. For example, the google
  // meet PWA has an app name "_crx_kjgfgldnnfoeklkmfkjfagphfepbbdan". We will
  // send this to ash where it will use "kjgfgldnnfoeklkmfkjfagphfepbbdan" to
  // query the app service for data.
  const std::string app_name = session_window->app_name;
  if (!app_name.empty()) {
    new_session_window->app_name = app_name;

    // App browsers will use the new app id to get an app icon and app
    // title. We don't need to look at tab data for these.
    return new_session_window;
  }

  // TODO(http://b/329152636): The active tab index
  // (`SessionWindow::selected_tab_index`) should be included in
  // the list of urls and be the first one. For now use the first tab's
  // title.
  std::string active_tab_title;
  std::vector<GURL> tab_urls;
  const auto& tabs = session_window->tabs;
  for (const std::unique_ptr<sessions::SessionTab>& tab : tabs) {
    const auto& navigations = tab->navigations;
    const int index = tab->current_navigation_index;
    // `index` can actually be larger than the size of `navigations`. See
    // `sessions::SessionTab::current_navigation_index` for more details.
    if (navigations.size() <= static_cast<size_t>(index)) {
      continue;
    }

    const sessions::SerializedNavigationEntry& entry = navigations[index];

    // Use the tab title if possible. Otherwise we will default to the app
    // title, "Chrome".
    if (active_tab_title.empty() && !entry.title().empty()) {
      active_tab_title = base::UTF16ToUTF8(entry.title());
    }

    tab_urls.push_back(entry.original_request_url());

    // We only show five favicons maximum so we can stop once we reach that
    // amount.
    if (tab_urls.size() >= kMaxUrls) {
      break;
    }
  }

  new_session_window->active_tab_title = active_tab_title;
  new_session_window->urls = tab_urls;
  new_session_window->tab_count = tabs.size();
  return new_session_window;
}

}  // namespace full_restore
