// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "components/query_tiles/switches.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/QueryTileUtils_jni.h"
#endif

namespace query_tiles {

// Issue 1076964: Currently the variation service can be only reached in full
// browser mode. Ensure the fetcher task launches OnFullBrowserLoaded.
// TODO(hesen): Work around store/get country code in reduce mode.
// static
std::string GetCountryCode() {
  std::string country_code;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(query_tiles::switches::kQueryTilesCountryCode)) {
    country_code = command_line->GetSwitchValueASCII(
        query_tiles::switches::kQueryTilesCountryCode);
    if (!country_code.empty())
      return country_code;
  }

  if (!g_browser_process)
    return country_code;

  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (!country_code.empty())
      return country_code;
    country_code = variations_service->GetLatestCountry();
  }
  return country_code;
}

bool IsQueryTilesEnabled() {
  return (!base::FeatureList::IsEnabled(
              query_tiles::features::kQueryTilesDisableCountryOverride) &&
          query_tiles::features::IsQueryTilesEnabledForCountry(
              GetCountryCode())) ||
         base::FeatureList::IsEnabled(query_tiles::features::kQueryTiles);
}

#if BUILDFLAG(IS_ANDROID)
static jboolean JNI_QueryTileUtils_IsQueryTilesEnabled(JNIEnv* env) {
  return IsQueryTilesEnabled();
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_QueryTileUtils_GetCountryCode(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetCountryCode());
}
#endif

}  // namespace query_tiles
