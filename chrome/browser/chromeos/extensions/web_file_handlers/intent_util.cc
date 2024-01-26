// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/web_file_handlers/intent_util.h"

#include "base/files/safe_base_name.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

namespace extensions {

std::vector<base::SafeBaseName> GetBaseNamesForIntent(
    const apps::Intent& intent) {
  std::vector<base::SafeBaseName> base_names;
  for (const auto& file : intent.files) {
    std::optional<base::SafeBaseName> optional_base_name;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    optional_base_name = file->file_name;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    optional_base_name = base::SafeBaseName::Create(file->url.path());
#endif
    // Launch requires that every file have a base name.
    if (!optional_base_name.has_value() ||
        optional_base_name.value().path().empty()) {
      return {};
    }

    base_names.emplace_back(optional_base_name.value());
  }
  return base_names;
}

bool IsLegacyQuickOfficeExtension(const Extension& extension) {
  return extension_misc::IsQuickOfficeExtension(extension.id()) &&
         !extensions::WebFileHandlers::SupportsWebFileHandlers(extension);
}

}  // namespace extensions
