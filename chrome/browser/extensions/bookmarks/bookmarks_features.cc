// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmarks/bookmarks_features.h"

#include "base/feature_list.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

BASE_FEATURE(kEnforceBookmarkVisibilityOnExtensionsAPI,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace extensions
