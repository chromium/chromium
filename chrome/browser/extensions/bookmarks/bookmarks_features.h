// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_FEATURES_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_FEATURES_H_

#include "base/feature_list.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Whether bookmark visibility is strictly enforced (for read and write
// operations) on the extensions API.
BASE_DECLARE_FEATURE(kEnforceBookmarkVisibilityOnExtensionsAPI);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_FEATURES_H_
