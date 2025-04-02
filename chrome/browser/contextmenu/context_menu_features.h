// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTMENU_CONTEXT_MENU_FEATURES_H_
#define CHROME_BROWSER_CONTEXTMENU_CONTEXT_MENU_FEATURES_H_

#include "base/feature_list.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kContextMenuEmptySpace);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features

#endif  // CHROME_BROWSER_CONTEXTMENU_CONTEXT_MENU_FEATURES_H_
