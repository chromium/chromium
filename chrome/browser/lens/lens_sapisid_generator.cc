// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_sapisid_generator.h"

#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#include "components/optimization_guide/core/optimization_guide_library_holder.h"
#endif

namespace lens {

DISABLE_CFI_DLSYM
std::optional<std::string> GenerateSapisidHash(
    const std::string& email,
    const std::string& sapisid_cookie,
    const std::string& origin,
    base::Time timestamp) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::OptimizationGuideLibraryHolder* loader =
      optimization_guide::OptimizationGuideLibraryHolder::GetInstance();
  if (!loader) {
    return std::nullopt;
  }
  typedef int (*GenerateFunc)(const char*, const char*, const char*, int64_t,
                              char**);
  typedef void (*FreeFunc)(char*);

  GenerateFunc generate_func = reinterpret_cast<GenerateFunc>(
      loader->GetFunctionPointer("GenerateSapisidHash"));
  FreeFunc free_func =
      reinterpret_cast<FreeFunc>(loader->GetFunctionPointer("FreeSapisidHash"));

  if (!generate_func || !free_func) {
    return std::nullopt;
  }

  char* out_hash = nullptr;
  base::TimeTicks start_time = base::TimeTicks::Now();
  int result =
      generate_func(email.c_str(), sapisid_cookie.c_str(), origin.c_str(),
                    timestamp.InMillisecondsSinceUnixEpoch(), &out_hash);
  if (result != 0 || !out_hash) {
    if (out_hash) {
      free_func(out_hash);
    }
    return std::nullopt;
  }

  base::UmaHistogramMicrosecondsTimes(
      "Lens.IdentityDelegation.TimeToGenerateSapisidHash",
      base::TimeTicks::Now() - start_time);
  std::string hash_str(out_hash);
  free_func(out_hash);
  return hash_str;
#else
  return std::nullopt;
#endif
}

}  // namespace lens
