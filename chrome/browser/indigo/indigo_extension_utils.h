// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_EXTENSION_UTILS_H_
#define CHROME_BROWSER_INDIGO_INDIGO_EXTENSION_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "ui/base/webui/resource_path.h"

namespace base {
class DictValue;
}

namespace indigo_extension_utils {

// Returns the manifest for the extension used to implement Indigo's image
// replacement content.
std::string GetManifest();

// Returns localized strings used by the Indigo extension.
base::DictValue GetStrings();

// Returns resources (scripts, styles, images) used by the Indigo extension.
base::span<const webui::ResourcePath> GetResources();

}  // namespace indigo_extension_utils

#endif  // CHROME_BROWSER_INDIGO_INDIGO_EXTENSION_UTILS_H_
