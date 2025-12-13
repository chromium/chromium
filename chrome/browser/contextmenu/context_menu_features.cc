// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextmenu/context_menu_features.h"

namespace features {

// Enables the empty space context menu on Clank.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kContextMenuEmptySpace, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
