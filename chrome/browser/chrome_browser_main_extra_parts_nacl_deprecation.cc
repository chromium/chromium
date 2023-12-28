// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/common/pref_names.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"

namespace {

#if BUILDFLAG(ENABLE_NACL)
bool ShouldNaClBeAllowed() {
  // Enabled by policy.
#if BUILDFLAG(IS_CHROMEOS)
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kNativeClientForceAllowed)) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kNaclAllow);
}
#endif

}  // namespace

BASE_FEATURE(kNaclAllow,
             "NaclAllow",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

void ChromeBrowserMainExtraPartsNaclDeprecation::PostEarlyInitialization() {
#if BUILDFLAG(ENABLE_NACL)
  if (!ShouldNaClBeAllowed()) {
    DisallowNacl();
  }
#endif
}
