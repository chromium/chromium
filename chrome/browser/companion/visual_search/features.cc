// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace companion::visual_search {
namespace features {

BASE_FEATURE(kVisualSearchSuggestions,
             "VisualSearchSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace switches {

const char kVisualSearchConfigForCompanion[] = "visual-search-config-companion";

absl::optional<std::string> GetVisualSearchConfigForCompanionOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kVisualSearchConfigForCompanion)) {
    return absl::optional<std::string>(
        command_line->GetSwitchValueASCII(kVisualSearchConfigForCompanion));
  }
  return absl::nullopt;
}

}  // namespace switches
}  // namespace companion::visual_search
