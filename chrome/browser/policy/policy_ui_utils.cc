// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_ui_utils.h"

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/browser/webui/json_generation.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/version/version_loader.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/version/version_util_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/android_about_app_info.h"
#endif

namespace policy {

JsonGenerationParams GetChromeMetadataParams(
    const std::string& application_name) {
  std::optional<std::string> cohort_name;
#if BUILDFLAG(IS_WIN)
  std::u16string cohort_version_info =
      version_utils::win::GetCohortVersionInfo();
  if (!cohort_version_info.empty()) {
    cohort_name = base::StringPrintf(
        " %s", base::UTF16ToUTF8(cohort_version_info).c_str());
  }
#endif
  std::optional<std::string> os_name;
  std::optional<std::string> platform_name;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  platform_name = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
#elif BUILDFLAG(IS_MAC)
  os_name = base::mac::GetOSDisplayName();
#else
  os_name = version_info::GetOSType();
#if BUILDFLAG(IS_WIN)
  os_name = os_name.value() + " " + version_utils::win::GetFullWindowsVersion();
#elif BUILDFLAG(IS_ANDROID)
  os_name = os_name.value() + " " + AndroidAboutAppInfo::GetOsInfo();
#endif
#endif
  policy::JsonGenerationParams params;
  params.with_application_name(application_name)
      .with_channel_name(
          chrome::GetChannelName(chrome::WithExtendedStable(true)))
      .with_processor_variation(
          l10n_util::GetStringUTF8(VersionUI::VersionProcessorVariation()));

  if (cohort_name) {
    params.with_cohort_name(cohort_name.value());
  }

  if (os_name) {
    params.with_os_name(os_name.value());
  }

  if (platform_name) {
    params.with_platform_name(platform_name.value());
  }
  return params;
}
}  // namespace policy
