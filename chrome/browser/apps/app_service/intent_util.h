// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_

#include <vector>

#include "components/arc/mojom/intent_common.mojom.h"
#include "components/arc/mojom/intent_helper.mojom-forward.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace arc {
class IntentFilter;
}

namespace base {
class FilePath;
}

namespace apps_util {
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

// Convert between App Service and ARC IntentFilters.
arc::IntentFilter CreateArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter);
apps::mojom::IntentFilterPtr ConvertArcIntentFilter(
    const arc::IntentFilter& arc_intent_filter);

// Convert between App Service and ARC Intents.
arc::mojom::IntentInfoPtr CreateArcIntent(const apps::mojom::IntentPtr& intent);
base::flat_map<std::string, std::string> CreateArcIntentExtras(
    const apps::mojom::IntentPtr& intent);

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
