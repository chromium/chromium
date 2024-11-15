// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api_test_util.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"

namespace extensions::api_test_util {

const Extension* GetSingleLoadedExtension(Profile* profile,
                                          std::string& message) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);

  const Extension* result = nullptr;
  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if (extension->location() == mojom::ManifestLocation::kComponent) {
      continue;
    }

    if (result != nullptr) {
      // TODO(yoz): this is misleading; it counts component extensions.
      message = base::StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(registry->enabled_extensions().size()));
      return nullptr;
    }

    result = extension.get();
  }

  if (!result) {
    message = "extension pointer is null.";
    return nullptr;
  }
  return result;
}

}  // namespace extensions::api_test_util
