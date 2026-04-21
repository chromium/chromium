// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include <array>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace default_browser {

bool IsDefaultBrowserFrameworkEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserFramework);
}

bool IsDefaultBrowserChangedOsNotificationEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserChangedOsNotification);
}

bool IsDefaultBrowserPromptSurfacesEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromptSurfaces);
}

DefaultBrowserPromptSurface GetDefaultBrowserPromptSurface() {
  if (!IsDefaultBrowserPromptSurfacesEnabled()) {
    return DefaultBrowserPromptSurface::kInfobar;
  }

  return kDefaultBrowserPromptSurfaceParam.Get();
}

BASE_FEATURE(kDefaultBrowserFramework, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPromptSurfaces, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr inline auto kDefaultBrowserPromptSurfaceOptions =
    std::to_array<base::FeatureParam<DefaultBrowserPromptSurface>::Option>({
        {DefaultBrowserPromptSurface::kInfobar, "infobar"},
        {DefaultBrowserPromptSurface::kBubbleDialog, "bubble_dialog"},
        {DefaultBrowserPromptSurface::kModalDialogWithSettingsIllustration,
         "modal_dialog_with_settings_illustration"},
        {DefaultBrowserPromptSurface::kModalDialogWithoutSettingsIllustration,
         "modal_dialog_without_settings_illustration"},
    });

BASE_FEATURE_ENUM_PARAM(DefaultBrowserPromptSurface,
                        kDefaultBrowserPromptSurfaceParam,
                        &kDefaultBrowserPromptSurfaces,
                        "prompt_surface",
                        DefaultBrowserPromptSurface::kInfobar,
                        kDefaultBrowserPromptSurfaceOptions);

BASE_FEATURE(kPerformDefaultBrowserCheckValidations,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserChangedOsNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserVisualGuidedSetter,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace default_browser
