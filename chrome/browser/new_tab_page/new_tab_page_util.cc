// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/search/ntp_features.h"
#include "components/variations/service/variations_service.h"

namespace {
// TODO(b/244303302): Have ItemSuggest filter out unsupported languages
// server-side.
constexpr const char* kDriveLocales[] = {
    "af",  "am", "ar",    "az",    "bg",     "bn",    "ca",    "cs",    "da",
    "de",  "el", "en-GB", "en-US", "es-419", "es",    "et",    "fa",    "fi",
    "fil", "fr", "gu",    "he",    "hi",     "hr",    "hu",    "hy",    "id",
    "is",  "it", "ja",    "ka",    "kk",     "km",    "kn",    "ko",    "ky",
    "lo",  "lt", "lv",    "mk",    "ml",     "mn",    "mr",    "ms",    "my",
    "ne",  "nl", "no",    "pa",    "pl",     "pt-BR", "pt-PT", "ro",    "ru",
    "si",  "sk", "sl",    "sq",    "sr",     "sv",    "sw",    "ta",    "te",
    "th",  "tr", "uk",    "ur",    "uz",     "vi",    "zh-CN", "zh-TW",
};

bool IsOsSupportedForRecipe() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

bool IsOsSupportedForCart() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

bool IsOsSupportedForDrive() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

std::string GetCountryCode() {
  std::string country_code;
  auto* variations_service = g_browser_process->variations_service();
  if (!variations_service)
    return country_code;
  country_code = variations_service->GetStoredPermanentCountry();
  return country_code.empty() ? variations_service->GetLatestCountry()
                              : country_code;
}

bool IsInUS() {
  return g_browser_process->GetApplicationLocale() == "en-US" &&
         GetCountryCode() == "us";
}

bool IsLocaleSupportedForDrive() {
  return std::find(std::begin(kDriveLocales), std::end(kDriveLocales),
                   g_browser_process->GetApplicationLocale()) !=
         std::end(kDriveLocales);
}
}  // namespace

// If feature is overridden manually or by finch, read the feature flag value.
// Otherwise filter by os, locale and country code.
bool IsRecipeTasksModuleEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          ntp_features::kNtpRecipeTasksModule.name)) {
    return base::FeatureList::IsEnabled(ntp_features::kNtpRecipeTasksModule);
  }
  return IsOsSupportedForRecipe() && IsInUS();
}

bool IsCartModuleEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          ntp_features::kNtpChromeCartModule.name)) {
    return base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule);
  }
  return IsOsSupportedForCart() && IsInUS();
}

bool IsDriveModuleEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          ntp_features::kNtpDriveModule.name)) {
    return base::FeatureList::IsEnabled(ntp_features::kNtpDriveModule);
  }
  return IsOsSupportedForDrive() && IsLocaleSupportedForDrive();
}
