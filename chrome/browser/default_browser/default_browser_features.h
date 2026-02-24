// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace default_browser {

enum class DefaultBrowserPromptSurface { kInfobar, kBubbleDialog };

// Returns whether the default browser framework feature flag is enabled.
bool IsDefaultBrowserFrameworkEnabled();

// Returns whether the default browser changed os notification feature flag is
// enabled.
bool IsDefaultBrowserChangedOsNotificationEnabled();

// Returns whether the experimental default browser prompt surfaces are enabled.
bool IsDefaultBrowserPromptSurfacesEnabled();

// Returns the UI surface to use for Default Browser Prompt. Defaults to Infobar
// if the `kDefaultBrowserFramework` feature is disabled.
DefaultBrowserPromptSurface GetDefaultBrowserPromptSurface();

BASE_DECLARE_FEATURE(kDefaultBrowserFramework);

// Enables the default browser prompt surfaces (e.g. invalidation, reprompt).
BASE_DECLARE_FEATURE(kDefaultBrowserPromptSurfaces);

// Enables the framework to perform additional checks when detecting default
// browser.
BASE_DECLARE_FEATURE(kPerformDefaultBrowserCheckValidations);

// Enables the framework to show Os Notification when Chrome is no longer the
// default browser.
// NOTE: This flag expect that `kDefaultBrowserFramework` is enabled first.
BASE_DECLARE_FEATURE(kDefaultBrowserChangedOsNotification);

BASE_DECLARE_FEATURE_PARAM(DefaultBrowserPromptSurface,
                           kDefaultBrowserPromptSurfaceParam);

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_
