// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

constexpr auto striped_symbols =
    base::MakeFixedFlatSet<char>({' ', '\t', '\n', '.', ','});

}

size_t NonWhitespaceAndSymbolsLength(const std::u16string& text,
                                     gfx::Range selection_range) {
  size_t start = selection_range.start();
  size_t end = selection_range.end();
  if (start >= end || end > text.length()) {
    return 0;
  }

  while (start < end && (striped_symbols.contains(text[start]) ||
                         striped_symbols.contains(text[end - 1]))) {
    if (striped_symbols.contains(text[start])) {
      start++;
    } else {
      end--;
    }
  }

  return end - start;
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
