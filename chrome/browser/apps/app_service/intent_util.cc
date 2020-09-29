// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include "chrome/browser/apps/app_service/file_utils.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace apps_util {

// TODO(crbug.com/853604): Make this not link to file manager extension if
// possible.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types) {
  auto file_urls = apps::GetFileUrls(profile, file_paths);
  return CreateShareIntentFromFiles(file_urls, mime_types);
}

}  // namespace apps_util
