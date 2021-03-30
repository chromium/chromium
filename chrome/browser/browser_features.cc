// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

#include "build/chromeos_buildflags.h"

namespace features {

// Enables using the ClosedTabCache to instantly restore recently closed tabs
// using the "Reopen Closed Tab" button.
const base::Feature kClosedTabCache{"ClosedTabCache",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Destroy profiles when their last browser window is closed, instead of when
// the browser exits.
const base::Feature kDestroyProfileOnBrowserClose{
    "DestroyProfileOnBrowserClose", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables executing the browser commands sent by the NTP promos.
const base::Feature kPromoBrowserCommands{"PromoBrowserCommands",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Parameter name for the promo browser command ID provided along with
// kPromoBrowserCommands.
// The value of this parameter should be parsable as an unsigned integer and
// should map to one of the browser commands specified in:
// chrome/browser/promo_browser_command/promo_browser_command.mojom
const char kPromoBrowserCommandIdParam[] = "PromoBrowserCommandIdParam";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
const base::Feature kDoubleTapToZoomInTabletMode{
    "DoubleTapToZoomInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Adds an item to the context menu that copies a link to the page with the
// selected text highlighted.
const base::Feature kCopyLinkToText{"CopyLinkToText",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// Enables taking snapshots of the user data directory after a major
// milestone update and restoring them after a version rollback.
const base::Feature kUserDataSnapshot{"UserDataSnapshot",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
