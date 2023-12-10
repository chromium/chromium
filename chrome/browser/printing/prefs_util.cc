// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/prefs_util.h"

#include <string>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "printing/printing_features.h"
#endif

namespace printing {

#if BUILDFLAG(ENABLE_OOP_PRINTING)
namespace {

bool CheckOopPolicy() {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state &&
      local_state->HasPrefPath(prefs::kOopPrintDriversAllowedByPolicy)) {
    return local_state->GetBoolean(prefs::kOopPrintDriversAllowedByPolicy);
  }
  return true;
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

absl::optional<gfx::Size> ParsePaperSizeDefault(const PrefService& prefs) {
  if (!prefs.HasPrefPath(prefs::kPrintingPaperSizeDefault))
    return absl::nullopt;

  const base::Value::Dict& paper_size_dict =
      prefs.GetDict(prefs::kPrintingPaperSizeDefault);

  if (paper_size_dict.empty())
    return absl::nullopt;

  const base::Value::Dict* custom_size_dict =
      paper_size_dict.FindDict(kPaperSizeCustomSize);
  if (custom_size_dict) {
    return gfx::Size(*custom_size_dict->FindInt(kPaperSizeWidth),
                     *custom_size_dict->FindInt(kPaperSizeHeight));
  }

  const std::string* name = paper_size_dict.FindString(kPaperSizeName);
  DCHECK(name);
  return ParsePaperSize(*name);
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
bool IsOopPrintingEnabled() {
  // First check feature flag.
  if (!base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    return false;
  }

  // Check for policy override.  Do no support dynamic refresh, cache and reuse
  // the value from the first check.
  static bool policy_override = CheckOopPolicy();
  return policy_override;
}

bool ShouldPrintJobOop() {
  return IsOopPrintingEnabled() &&
         features::kEnableOopPrintDriversJobPrint.Get();
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing
