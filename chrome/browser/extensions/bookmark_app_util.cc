// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_util.h"

#include <map>
#include <memory>
#include <string_view>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace {

// A preference used to indicate that a bookmark apps is fully locally installed
// on this machine. The default value (i.e. if the pref is not set) is to be
// fully locally installed, so that hosted apps or bookmark apps created /
// synced before this pref existed will be treated as locally installed.
const char kPrefLocallyInstalled[] = "locallyInstalled";

}  // namespace

bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension) {
  return BookmarkAppIsLocallyInstalled(ExtensionPrefs::Get(context), extension);
}

bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension) {
  bool locally_installed;
  if (prefs->ReadPrefAsBoolean(extension->id(), kPrefLocallyInstalled,
                               &locally_installed)) {
    return locally_installed;
  }

  return true;
}

}  // namespace extensions
