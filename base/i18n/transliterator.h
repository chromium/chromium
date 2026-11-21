// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TRANSLITERATOR_H_
#define BASE_I18N_TRANSLITERATOR_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/i18n/base_i18n_export.h"

// Provides a wrapper for Unicode string transliteration.

namespace base {
namespace i18n {

class BASE_I18N_EXPORT Transliterator {
 public:
  virtual ~Transliterator() = default;
  // Transliterates `text`. Unpaired UTF-16 surrogates in `text` may be
  // replaced with U+FFFD.
  virtual std::u16string Transliterate(std::u16string_view text) const = 0;
};

// Creates a transliterator for `id`: a supported ICU transform ID, `;`-chained
// sequence of IDs, or pre-baked BCP-47-T transform ID in
// `//tools/crates/icu4x_datagen/config.toml` (including Chromium's custom
// `und-t-und-x0-{afill,afillde,dropacc,xconf,rmhyph,diacstr}` rule sets; see
// `//base/i18n/translit/custom_transliterator.rs`). Hits `DCHECK` and returns
// `nullptr` for unsupported IDs.
BASE_I18N_EXPORT std::unique_ptr<Transliterator> CreateTransliterator(
    std::string_view id);

}  // namespace i18n
}  // namespace base

#endif  // BASE_I18N_TRANSLITERATOR_H_
