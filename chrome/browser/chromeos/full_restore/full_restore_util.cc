// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/session_types.h"
#include "content/public/common/url_constants.h"

namespace full_restore {

namespace {
constexpr size_t kMaxUrls = 5u;
}  // namespace

crosapi::mojom::SessionWindowPtr ToSessionWindowPtr(
    const sessions::SessionWindow& session_window,
    uint64_t lacros_profile_id) {
  crosapi::mojom::SessionWindowPtr new_session_window =
      crosapi::mojom::SessionWindow::New();
  new_session_window->window_id = session_window.window_id.id();
  new_session_window->profile_id = lacros_profile_id;

  // App browsers app ID is the same as regular chrome browsers. To get the
  // correct icon and title from the app service, we need to find the app
  // name and remove the "_crx_", then use that result. For example, the google
  // meet PWA has an app name "_crx_kjgfgldnnfoeklkmfkjfagphfepbbdan". We will
  // send this to ash where it will use "kjgfgldnnfoeklkmfkjfagphfepbbdan" to
  // query the app service for data.
  if (!session_window.app_name.empty()) {
    new_session_window->app_name = session_window.app_name;

    // App browsers will use the new app id to get an app icon and app
    // title. We don't need to look at tab data for these.
    return new_session_window;
  }

  // If there is no selected tab index or it is invalid, we can just pass the
  // URLs as they are. If the selected tab index is one of the first five
  // elements, then we place that URL at the front and place the remaining
  // four URLs afterwards. Otherwise, we put the selected tab index at the
  // front and insert the first four URLs after it.
  std::string active_tab_title;
  std::vector<GURL> tab_urls;
  const std::vector<std::unique_ptr<sessions::SessionTab>>& tabs =
      session_window.tabs;

  auto maybe_add_display_tab =
      [&tab_urls, &active_tab_title](sessions::SessionTab* tab) -> void {
    const auto& navigations = tab->navigations;
    const int index = tab->current_navigation_index;

    // `index` can actually be larger than the size of `navigations`. See
    // `sessions::SessionTab::current_navigation_index` for more details.
    if (navigations.size() > static_cast<size_t>(index)) {
      const sessions::SerializedNavigationEntry& entry = navigations[index];

      // Use the tab title if possible. If no tab title is available and it is a
      // chrome WebUI, use the host piece (history, extensions, etc.). Otherwise
      // we will default to the app title, "Chrome".
      if (active_tab_title.empty()) {
        active_tab_title = base::UTF16ToUTF8(entry.title());
        if (active_tab_title.empty() &&
            entry.original_request_url().SchemeIs(content::kChromeUIScheme)) {
          active_tab_title = entry.original_request_url().host_piece();
        }
      }

      tab_urls.push_back(entry.original_request_url());
    }
  };

  // Add the selected tab first if possible.
  const int selected_tab_index = session_window.selected_tab_index;
  if (selected_tab_index > -1 &&
      selected_tab_index < static_cast<int>(tabs.size())) {
    maybe_add_display_tab(tabs[selected_tab_index].get());
  }

  // Add the other tabs in order until there are no more tabs or we reach the
  // limit.
  for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
    if (i == selected_tab_index) {
      continue;
    }
    maybe_add_display_tab(tabs[i].get());

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
