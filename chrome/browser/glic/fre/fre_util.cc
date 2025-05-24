// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/fre_util.h"

#include <algorithm>
#include <cstddef>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_ui/theme_util.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/language/core/common/language_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/url_util.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace glic {

GURL GetFreURL(Profile* profile) {
  // Use the corresponding command line argument as the URL, if available.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool has_glic_fre_url = command_line->HasSwitch(::switches::kGlicFreURL);
  GURL base_url =
      GURL(has_glic_fre_url
               ? command_line->GetSwitchValueASCII(::switches::kGlicFreURL)
               : features::kGlicFreURL.Get());
  if (base_url.is_empty()) {
    LOG(ERROR) << "No glic fre url";
  }

  GURL fre_url = base_url;
  // Add the hotkey configuration to the URL as a query parameter.
  std::string hotkey_param_value;
#if !BUILDFLAG(IS_MAC)
  hotkey_param_value = GetHotkeyString();
#else
  hotkey_param_value = GetLongFormMacHotkeyString();
#endif

  if (!hotkey_param_value.empty()) {
    fre_url = net::AppendOrReplaceQueryParameter(fre_url, "hotkey",
                                                 hotkey_param_value);
  }

  // Add the current Chrome theme to the URL as a query parameter.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  std::string theme_value = UseDarkMode(theme_service) ? "dark" : "light";
  fre_url = net::AppendOrReplaceQueryParameter(fre_url, "theme", theme_value);

  // Localize to Chrome UI language.
  std::string locale = g_browser_process->GetApplicationLocale();
  language::ToTranslateLanguageSynonym(&locale);
  return net::AppendOrReplaceQueryParameter(fre_url, "hl", locale);
}

content::StoragePartitionConfig GetFreStoragePartitionConfig(
    content::BrowserContext* browser_context) {
  // This storage partition must match the partition attribute in
  // chrome/browser/resources/glic_fre/fre.html: "glicfrepart".
  return content::StoragePartitionConfig::Create(
      browser_context, "glic-fre",
      /*partition_name=*/"glicfrepart",
      /*in_memory=*/true);
}

}  // namespace glic
