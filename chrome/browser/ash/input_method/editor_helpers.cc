// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_helpers.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

namespace {

constexpr auto kAllowedLanguagesForShowingL10nStrings =
    base::MakeFixedFlatSet<std::string_view>({"de", "en", "en-GB", "fr", "ja"});

}

std::string GetSystemLocale() {
  return g_browser_process != nullptr
             ? g_browser_process->GetApplicationLocale()
             : "";
}

bool ShouldUseL10nStrings() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled() ||
         (chromeos::features::IsOrcaInternationalizeEnabled() &&
          base::Contains(kAllowedLanguagesForShowingL10nStrings,
                         GetSystemLocale()));
}

}  // namespace ash::input_method
