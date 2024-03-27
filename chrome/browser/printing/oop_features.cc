// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/oop_features.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/printing/prefs_util.h"
#include "printing/printing_features.h"
#endif

namespace printing {

#if BUILDFLAG(ENABLE_OOP_PRINTING)
bool IsOopPrintingEnabled() {
  // First check feature flag.
  if (!base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    return false;
  }

  // Check for policy override.
  return OopPrintingPref().value_or(true);
}

bool ShouldPrintJobOop() {
  return IsOopPrintingEnabled() &&
         features::kEnableOopPrintDriversJobPrint.Get();
}

bool ShouldEarlyStartPrintBackendService() {
  return IsOopPrintingEnabled() &&
#if BUILDFLAG(IS_WIN)
         features::kEnableOopPrintDriversSingleProcess.Get() &&
#endif
         features::kEnableOopPrintDriversEarlyStart.Get();
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing
