// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/supported_locales.h"

#include <ranges>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "third_party/icu/source/common/unicode/uloc.h"

namespace base::i18n {
namespace {

// Checks whether there is an english translation for the locale. Some locales
// have only partial data present, this functions makes us skip them.
bool HasIcuData(std::string_view icu_locale) {
  std::u16string display_name;
  const int kBufferSize = 1024;
  UErrorCode error = U_ZERO_ERROR;

  int32_t count = uloc_getDisplayName(
      icu_locale.data(), "en", base::WriteInto(&display_name, kBufferSize),
      kBufferSize - 1, &error);
  if (!U_SUCCESS(error)) {
    return false;
  }
  return count > 0;
}

bool ShouldSkipLocale(const LanguageTag& locale_tag) {
  // Skip locales such as "en-001", "en-150", but keep "es-419".
  // The only locale with a region that is not country (length equals to 3 means
  // region codes that are not countries) that should be used in Chrome is
  // "es-419" (Spanish from Latin America). The others are skipped here as an
  // optimization to reduce the output size and prevent them to be used.
  if (locale_tag.region_subtag().size() == 3 &&
      locale_tag.region_subtag() != "419") {
    return true;
  }

  return false;
}

}  // namespace

const base::flat_set<LanguageTag>& GetSupportedIcuLocales() {
  static const base::NoDestructor<base::flat_set<LanguageTag>>
      kAvailableICULocales([]() {
        std::vector<LanguageTag> locales;
        int num_locales = uloc_countAvailable();
        for (int i = 0; i < num_locales; ++i) {
          std::string_view locale_name = uloc_getAvailable(i);
          std::optional<LanguageTag> locale_tag =
              GetLanguageTagFromString(locale_name);
          if (!locale_tag || !HasIcuData(locale_name) ||
              ShouldSkipLocale(*locale_tag)) {
            continue;
          }
          locales.push_back(*locale_tag);

          // Add the parents as ICU locales as well.
          for (std::optional<LanguageTag> parent = locale_tag->GetParentTag();
               parent; parent = parent->GetParentTag()) {
            locales.push_back(*parent);
          }
        }

        locales.shrink_to_fit();
        return base::flat_set<LanguageTag>(std::move(locales));
      }());

  return *kAvailableICULocales;
}

}  // namespace base::i18n
