// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_FEATURES_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// This feature controls the loading of bookmarks from initial_preferences on
// browser first run.
BASE_DECLARE_FEATURE(kBookmarksImportOnFirstRun);

}  // namespace features

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_FEATURES_H_
