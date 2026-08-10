// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/normalizer.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/i18n/icubridge/icu_bridge_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/ComposingNormalizer.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/DecomposingNormalizer.d.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/DecomposingNormalizer.hpp"

namespace base::i18n {

namespace {

// ICU4X Cached Normalizers
const icu4x::ComposingNormalizer* GetIcu4xNfcNormalizer() {
  static const base::NoDestructor<std::unique_ptr<icu4x::ComposingNormalizer>>
      normalizer(icu4x::ComposingNormalizer::create_nfc());
  return normalizer->get();
}

const icu4x::ComposingNormalizer* GetIcu4xNfkcNormalizer() {
  static const base::NoDestructor<std::unique_ptr<icu4x::ComposingNormalizer>>
      normalizer(icu4x::ComposingNormalizer::create_nfkc());
  return normalizer->get();
}

const icu4x::DecomposingNormalizer* GetIcu4xNfdNormalizer() {
  static const base::NoDestructor<std::unique_ptr<icu4x::DecomposingNormalizer>>
      normalizer(icu4x::DecomposingNormalizer::create_nfd());
  return normalizer->get();
}

// ICU4C Cached Normalizers (raw pointers as they return non-owned singletons)
const icu::Normalizer2* GetIcu4cNfcNormalizer() {
  static const icu::Normalizer2* const normalizer = [] {
    UErrorCode status = U_ZERO_ERROR;
    return icu::Normalizer2::getNFCInstance(status);
  }();
  return normalizer;
}

const icu::Normalizer2* GetIcu4cNfdNormalizer() {
  static const icu::Normalizer2* const normalizer = [] {
    UErrorCode status = U_ZERO_ERROR;
    return icu::Normalizer2::getNFDInstance(status);
  }();
  return normalizer;
}

const icu::Normalizer2* GetIcu4cNfkcNormalizer() {
  static const icu::Normalizer2* const normalizer = [] {
    UErrorCode status = U_ZERO_ERROR;
    return icu::Normalizer2::getNFKCInstance(status);
  }();
  return normalizer;
}

template <typename Icu4xNormalizerT>
std::string NormalizeUtf8(const Icu4xNormalizerT* normalizer,
                          std::string_view utf8_input) {
  CHECK(normalizer) << "Normalizer must not be null.";
  return normalizer->normalize(utf8_input);
}

// Concrete hidden normalizer implementations
class Icu4xNormalizer : public IcuBridge::Normalizer {
 public:
  Icu4xNormalizer() = default;
  ~Icu4xNormalizer() override = default;

  std::u16string Normalize(NormalizationForm normalization_form,
                           std::u16string_view input) const override {
    std::string utf8_input = base::UTF16ToUTF8(input);
    std::string utf8_normalized;

    switch (normalization_form) {
      case NormalizationForm::NFC:
        utf8_normalized = NormalizeUtf8(GetIcu4xNfcNormalizer(), utf8_input);
        break;
      case NormalizationForm::NFKC: {
        utf8_normalized = NormalizeUtf8(GetIcu4xNfkcNormalizer(), utf8_input);
        break;
      }
      case NormalizationForm::NFD: {
        utf8_normalized = NormalizeUtf8(GetIcu4xNfdNormalizer(), utf8_input);
        break;
      }
    }

    return base::UTF8ToUTF16(utf8_normalized);
  }
};

class Icu4cNormalizer : public IcuBridge::Normalizer {
 public:
  Icu4cNormalizer() = default;
  ~Icu4cNormalizer() override = default;

  std::u16string Normalize(NormalizationForm normalization_form,
                           std::u16string_view input) const override {
    const icu::Normalizer2* normalizer = nullptr;

    switch (normalization_form) {
      case NormalizationForm::NFC:
        normalizer = GetIcu4cNfcNormalizer();
        break;
      case NormalizationForm::NFD:
        normalizer = GetIcu4cNfdNormalizer();
        break;
      case NormalizationForm::NFKC:
        normalizer = GetIcu4cNfkcNormalizer();
        break;
    }

    CHECK(normalizer) << "Normalizer must not be null.";
    icu::UnicodeString unicode_input(input.data(),
                                     static_cast<int32_t>(input.length()));
    icu::UnicodeString unicode_normalized;
    UErrorCode status = U_ZERO_ERROR;
    normalizer->normalize(unicode_input, unicode_normalized, status);

    if (U_FAILURE(status)) {
      return std::u16string(input);
    }

    return UnicodeStringToString16(unicode_normalized);
  }
};

}  // namespace

// Factory functions exposing the normalizers to IcuBridge
std::unique_ptr<IcuBridge::Normalizer> CreateIcu4xNormalizer(
    base::PassKey<IcuBridge> pass_key) {
  return std::make_unique<Icu4xNormalizer>();
}

std::unique_ptr<IcuBridge::Normalizer> CreateIcu4cNormalizer(
    base::PassKey<IcuBridge> pass_key) {
  return std::make_unique<Icu4cNormalizer>();
}

}  // namespace base::i18n
