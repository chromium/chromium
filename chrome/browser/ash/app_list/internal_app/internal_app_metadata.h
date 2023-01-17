// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
#define CHROME_BROWSER_ASH_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_

#include <string>
#include <vector>

#include "ui/gfx/image/image_skia.h"

class Profile;

namespace apps {
enum class BuiltInAppName;
}

namespace app_list {

// Metadata about an internal app.
// Internal apps are these Chrome OS special apps, e.g. Settings, or these apps
// can run in Chrome OS directly, e.g. Keyboard Shortcut Viewer.
struct InternalApp {
  const char* app_id;

  // Name of the app.
  int name_string_resource_id = 0;

  int icon_resource_id = 0;

  // Can show as a suggested app.
  bool recommendable;

  // Can be searched.
  bool searchable;

  // Can show in launcher apps grid.
  bool show_in_launcher;

  apps::BuiltInAppName internal_app_name;

  // The string used for search query in addition to the name.
  int searchable_string_resource_id = 0;
};

// Returns a list of Chrome OS internal apps, which are searchable in launcher
// for |profile|.
const std::vector<InternalApp>& GetInternalAppList(const Profile* profile);

// Returns true if the app should only be shown as a suggestion chip.
bool IsSuggestionChip(const std::string& app_id);

// Returns InternalApp by |app_id|.
// Returns nullptr if |app_id| does not correspond to an internal app.
const InternalApp* FindInternalApp(const std::string& app_id);

// Returns true if |app_id| corresponds to an internal app.
bool IsInternalApp(const std::string& app_id);

// Returns the number of internal apps which can show in launcher.
// If |apps_name| is not nullptr, it will be the concatenated string of these
// internal apps' name.
size_t GetNumberOfInternalAppsShowInLauncherForTest(std::string* apps_name,
                                                    const Profile* profile);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
