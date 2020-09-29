// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_

#include <vector>

#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace base {
class FilePath;
}

namespace apps_util {
// Create an intent struct from the file paths and mime types
// of a list of files.
// This util has to live under chrome/ because it uses fileapis
// and cannot be inlucded in components/.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types);
}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
