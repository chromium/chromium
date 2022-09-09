// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_common_utils.h"

#include <algorithm>
#include <string>

#include "chrome/common/url_constants.h"
#include "components/sessions/core/session_types.h"
#include "url/gurl.h"

bool ShouldTrackURLForRestore(const GURL& url) {
  return url.is_valid() &&
         !(url.SchemeIs(content::kChromeUIScheme) &&
           (url.host_piece() == chrome::kChromeUIQuitHost ||
            url.host_piece() == chrome::kChromeUIRestartHost));
}

int GetNavigationIndexToSelect(const sessions::SessionTab& tab) {
  DCHECK(!tab.navigations.empty());
  const int selected_index =
      std::max(0, std::min(tab.current_navigation_index,
                           static_cast<int>(tab.navigations.size() - 1)));

  // After user sign out, Chrome may navigate to the setting page from the
  // sign out page asynchronously. The browser may be closed before the
  // navigation callback finished.
  std::string setting_page_url = std::string(chrome::kChromeUISettingsURL);
  std::string sign_out_page_url =
      setting_page_url + std::string(chrome::kSignOutSubPage);
  if (selected_index > 0 &&
      tab.navigations[selected_index].virtual_url().spec() ==
          sign_out_page_url &&
      tab.navigations[selected_index - 1].virtual_url().spec() ==
          setting_page_url) {
    return selected_index - 1;
  }

  return selected_index;
}
