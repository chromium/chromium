// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_util.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace glic {

GURL DecorateGlicOptInUrl(Profile* profile, GURL url) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  // Add the hotkey configuration to the URL as a query parameter.
  std::string hotkey_param_value;
#if !BUILDFLAG(IS_MAC)
  hotkey_param_value = GetHotkeyString();
#else
  hotkey_param_value = GetLongFormMacHotkeyString();
#endif

  if (!hotkey_param_value.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, "hotkey", hotkey_param_value);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Add the current Chrome theme to the URL as a query parameter.
#if !BUILDFLAG(IS_ANDROID)
  const bool use_dark_mode =
      ThemeServiceFactory::GetForProfile(profile)->BrowserUsesDarkColors();
  std::string theme_value = use_dark_mode ? "dark" : "light";
  url = net::AppendOrReplaceQueryParameter(url, "theme", theme_value);
#endif

  url = MaybeAddMultiInstanceParameter(url);

  // Localize to Chrome UI language.
  return GetLocalizedGuestURL(url);
}

}  // namespace glic
