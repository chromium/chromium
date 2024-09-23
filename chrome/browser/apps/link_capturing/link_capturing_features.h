// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace apps::features {

#if BUILDFLAG(IS_CHROMEOS)
// Enables user link capturing on CrOS.
BASE_DECLARE_FEATURE(kLinkCapturingUiUpdate);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns true if the updated UX for link capturing needs to be shown. Only set
// to true if kPwaNavigationCapturing is enabled on desktop platforms, and
// kLinkCapturingUiUpdate on CrOS platforms.
bool ShouldShowLinkCapturingUX();

// Returns true if the `kPwaNavigationCapturing` flag is enabled with the
// reimplementation parameters set.
bool IsNavigationCapturingReimplEnabled();

}  // namespace apps::features

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
