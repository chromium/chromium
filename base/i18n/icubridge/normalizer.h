// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_NORMALIZER_H_
#define BASE_I18N_ICUBRIDGE_NORMALIZER_H_

#include <memory>
#include <string_view>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/types/pass_key.h"

namespace base::i18n {

// Interface for Unicode string normalization.
//
// Implementations of this interface (such as Icu4xNormalizer and
// Icu4cNormalizer) provide Unicode normalization capabilities via different
// underlying engines (ICU4X and ICU4C).
class BASE_I18N_EXPORT IcuBridge::Normalizer {
 public:
  // Unicode normalization forms.
  enum class NormalizationForm {
    // Canonical Decomposition, followed by Canonical Composition.
    NFC = 0,
    // Canonical Decomposition.
    NFD = 1,
    // Compatibility Decomposition, followed by Canonical Composition.
    NFKC = 2,
  };

  virtual ~Normalizer() = default;

  // Normalizes the given UTF-16 string view `input` according to the requested
  // `normalization_form` and returns the normalized UTF-16 string.
  virtual std::u16string Normalize(NormalizationForm normalization_form,
                                   std::u16string_view input) const = 0;
};

BASE_I18N_EXPORT std::unique_ptr<IcuBridge::Normalizer> CreateIcu4xNormalizer(
    base::PassKey<IcuBridge> pass_key);

BASE_I18N_EXPORT std::unique_ptr<IcuBridge::Normalizer> CreateIcu4cNormalizer(
    base::PassKey<IcuBridge> pass_key);

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_NORMALIZER_H_
