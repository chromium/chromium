// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include <array>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <vector>

#include "base/i18n/win/preferred_languages.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#endif

namespace default_browser {

bool IsDefaultBrowserFrameworkEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserFramework);
}

bool IsDefaultBrowserChangedOsNotificationEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(kDefaultBrowserChangedOsNotification);
#else
  return false;
#endif
}

bool IsDefaultBrowserPromptSurfacesEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(kDefaultBrowserPromptSurfaces);
#else
  return false;
#endif
}

bool IsVisualGuidedSetterDockingEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(kVisualGuidedSetterDocking);
#else
  return false;
#endif
}

DefaultBrowserPromptSurface GetDefaultBrowserPromptSurface() {
  if (!IsDefaultBrowserPromptSurfacesEnabled()) {
    return DefaultBrowserPromptSurface::kInfobar;
  }

  DefaultBrowserPromptSurface prompt_surface =
      kDefaultBrowserPromptSurfaceParam.Get();
#if BUILDFLAG(IS_WIN)
  // The modal prompt surface with settings illustration features an OS-level
  // diagram of Windows Settings with text in English. Because the visual UI
  // in Windows Settings reflects the user's primary Windows OS display language
  // (rather than Chrome's browser UI locale), display the illustration only
  // when the Windows display language is English. For non-English Windows UI
  // locales, fall back to the self-contained illustration-free modal dialog.
  if (prompt_surface ==
      DefaultBrowserPromptSurface::kModalDialogWithSettingsIllustration) {
    // When the visual guided setter is selected, use the illustration-free
    // modal dialog because guidance is provided by the visual guide instead
    // of an inline illustration.
    if (GetDefaultBrowserSetterType() ==
        DefaultBrowserSetterType::kVisualGuide) {
      return DefaultBrowserPromptSurface::
          kModalDialogWithoutSettingsIllustration;
    }

    std::string os_locale;
    const std::vector<base::i18n::LanguageTag> os_languages =
        base::i18n::GetUserPreferredUILanguageList();
    if (!os_languages.empty()) {
      os_locale = os_languages[0].tag_string();
    } else if (g_browser_process) {
      os_locale = g_browser_process->GetApplicationLocale();
    }

    if (!base::StartsWith(os_locale, "en",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return DefaultBrowserPromptSurface::
          kModalDialogWithoutSettingsIllustration;
    }
  }
#endif

  return prompt_surface;
}

DefaultBrowserSetterType GetDefaultBrowserSetterType() {
  if (!base::FeatureList::IsEnabled(kDefaultBrowserSetterSelection)) {
    return DefaultBrowserSetterType::kShellIntegration;
  }

  return kDefaultBrowserSetterParam.Get();
}

GURL GetDefaultBrowserVisualGuideURL() {
#if BUILDFLAG(IS_WIN)
  return GURL(kDefaultBrowserVisualGuideUrlParam.Get());
#else
  return GURL();
#endif
}

BASE_FEATURE(kDefaultBrowserFramework, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kDefaultBrowserSetterSelection, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualGuidedSetterDocking, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr inline auto kDefaultBrowserSetterSelectionOption =
    std::to_array<base::FeatureParam<DefaultBrowserSetterType>::Option>(
        {{DefaultBrowserSetterType::kShellIntegration, "shell_integration"},
         {DefaultBrowserSetterType::kVisualGuide, "visual_guide"}});

BASE_FEATURE_ENUM_PARAM(DefaultBrowserSetterType,
                        kDefaultBrowserSetterParam,
                        &kDefaultBrowserSetterSelection,
                        "setter_option",
                        DefaultBrowserSetterType::kShellIntegration,
                        kDefaultBrowserSetterSelectionOption);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE_PARAM(std::string,
                   kDefaultBrowserVisualGuideUrlParam,
                   &kDefaultBrowserSetterSelection,
                   chrome::kChromeUIDefaultBrowserVisualGuidedSetterURL);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kFindSettingsTimeout,
                   &kDefaultBrowserSetterSelection,
                   base::Seconds(5));
#endif

}  // namespace default_browser
