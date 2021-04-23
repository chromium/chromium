// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/arc/mojom/intent_common.mojom.h"
#include "components/arc/mojom/intent_helper.mojom-forward.h"

namespace arc {
class IntentFilter;
}
#endif

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace web_app {
class WebApp;
}  // namespace web_app

namespace apps_util {
// Create intent filters for |web_app| and append them to |target|.
void PopulateWebAppIntentFilters(
    const web_app::WebApp& web_app,
    std::vector<apps::mojom::IntentFilterPtr>& target);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Create an intent struct from the file paths and mime types
// of a list of files.
// This util has to live under chrome/ because it uses fileapis
// and cannot be included in components/.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types);

// Create an intent struct from the file paths, mime types
// of a list of files, and the share text and title.
// This util has to live under chrome/ because it uses fileapis
// and cannot be included in components/.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title);

// Create an intent struct from the arc intent and arc activity.
apps::mojom::IntentPtr CreateIntentForArcIntentAndActivity(
    arc::mojom::IntentInfoPtr arc_intent,
    arc::mojom::ActivityNamePtr activity);

base::flat_map<std::string, std::string> CreateArcIntentExtras(
    const apps::mojom::IntentPtr& intent);

// Convert between App Service and ARC Intents.
arc::mojom::IntentInfoPtr CreateArcIntent(const apps::mojom::IntentPtr& intent);

// Convert an apps::mojom::Intent struct to a string to call the LaunchIntent
// interface from arc::mojom::AppInstance. If |intent| has |ui_bypassed|, |url|
// or |data|, returns an empty string as these intents cannot be represented in
// string form.
std::string CreateLaunchIntent(const std::string& package_name,
                               const apps::mojom::IntentPtr& intent);

// Convert between App Service and ARC IntentFilters.
arc::IntentFilter CreateArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter);
apps::mojom::IntentFilterPtr ConvertArcIntentFilter(
    const arc::IntentFilter& arc_intent_filter);
#endif

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
