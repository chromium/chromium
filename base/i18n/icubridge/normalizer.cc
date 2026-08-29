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
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/ComposingNormalizer.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/DecomposingNormalizer.d.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/DecomposingNormalizer.hpp"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base::i18n {

#if BUILDFLAG(IS_ANDROID)
extern bool g_icu_initialized;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
void DumpNormalizationFailure(bool retry_worked) {
  std::string fallback_asset_path;
  base::FilePath asset_path;
  // This is the fall-back path used for tests.
  if (base::PathService::Get(base::DIR_ASSETS, &asset_path)) {
    asset_path = asset_path.AppendASCII("icudtl.dat");
    if (base::PathExists(asset_path)) {
      fallback_asset_path = asset_path.value();
    }
  }

  int64_t file_size = -1;
  std::string apk_path;
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd(base::android::OpenApkAsset("assets/icudtl.dat", &region));
  if (fd.is_valid()) {
    file_size = static_cast<int64_t>(region.size);
    base::FilePath target_path;
    if (base::ReadSymbolicLink(
            base::FilePath(base::StringPrintf("/proc/self/fd/%d", fd.get())),
            &target_path)) {
      apk_path = target_path.value();
    }
  }

  SCOPED_CRASH_KEY_STRING256("normalizer", "fallback_asset_path",
                             fallback_asset_path);
  SCOPED_CRASH_KEY_STRING256("normalizer", "apk_path", apk_path);
  SCOPED_CRASH_KEY_NUMBER("normalizer", "file_size", file_size);
  SCOPED_CRASH_KEY_BOOL("normalizer", "icu_init", g_icu_initialized);
  SCOPED_CRASH_KEY_BOOL("normalizer", "retry_worked", retry_worked);

  base::debug::DumpWithoutCrashing();
}
#endif  // BUILDFLAG(IS_ANDROID)

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

// Helper function to get an ICU4C normalizer instance with retry mechanism.
const icu::Normalizer2* GetIcu4cNormalizerWithRetry(
    IcuBridge::Normalizer::NormalizationForm normalization_form) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* inst = nullptr;
  switch (normalization_form) {
    case IcuBridge::Normalizer::NormalizationForm::NFC:
      inst = icu::Normalizer2::getNFCInstance(status);
      break;
    case IcuBridge::Normalizer::NormalizationForm::NFD:
      inst = icu::Normalizer2::getNFDInstance(status);
      break;
    case IcuBridge::Normalizer::NormalizationForm::NFKC:
      inst = icu::Normalizer2::getNFKCInstance(status);
      break;
  }

  if (U_FAILURE(status) || !inst) {
    status = U_ZERO_ERROR;
    switch (normalization_form) {
      case IcuBridge::Normalizer::NormalizationForm::NFC:
        inst = icu::Normalizer2::getNFCInstance(status);
        break;
      case IcuBridge::Normalizer::NormalizationForm::NFD:
        inst = icu::Normalizer2::getNFDInstance(status);
        break;
      case IcuBridge::Normalizer::NormalizationForm::NFKC:
        inst = icu::Normalizer2::getNFKCInstance(status);
        break;
    }
#if BUILDFLAG(IS_ANDROID)
    bool retry_worked = !U_FAILURE(status) && inst;
    DumpNormalizationFailure(retry_worked);
#endif
  }
  return inst;
}

// ICU4C Cached Normalizers (raw pointers as they return non-owned singletons)
const icu::Normalizer2* GetIcu4cNfcNormalizer() {
  static const icu::Normalizer2* const normalizer = GetIcu4cNormalizerWithRetry(
      IcuBridge::Normalizer::NormalizationForm::NFC);
  return normalizer;
}

const icu::Normalizer2* GetIcu4cNfdNormalizer() {
  static const icu::Normalizer2* const normalizer = GetIcu4cNormalizerWithRetry(
      IcuBridge::Normalizer::NormalizationForm::NFD);
  return normalizer;
}

const icu::Normalizer2* GetIcu4cNfkcNormalizer() {
  static const icu::Normalizer2* const normalizer = GetIcu4cNormalizerWithRetry(
      IcuBridge::Normalizer::NormalizationForm::NFKC);
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
#if BUILDFLAG(IS_ANDROID)
      DumpNormalizationFailure(/*retry_worked=*/false);
#endif  // BUILDFLAG(IS_ANDROID)
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
