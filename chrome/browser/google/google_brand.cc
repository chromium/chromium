// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_brand.h"

#include <stddef.h>

#include <array>
#include <compare>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/installer/util/google_update_settings.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/google/google_brand_chromeos.h"
#endif

namespace google_brand {

namespace {

// A fixed-length brand code stored by value, so constant tables of them need
// no relocations.
template <size_t N>
struct FixedBrandCode {
  // Needed for brace-init with string literals in `MakeFixedFlatSet`.
  // NOLINTNEXTLINE(google-explicit-constructor)
  consteval FixedBrandCode(const char (&s)[N + 1]) {
    auto input = base::span(s);
    CHECK(input.back() == '\0');
    base::span(chars).copy_from(input.template first<N>());
    CHECK(!std::string_view(chars).contains('\0'));
  }

  friend constexpr auto operator<=>(const FixedBrandCode& lhs,
                                    const FixedBrandCode& rhs) {
    return std::string_view(lhs.chars) <=> std::string_view(rhs.chars);
  }
  template <typename T>
    requires std::convertible_to<const T&, std::string_view>
  friend constexpr auto operator<=>(const FixedBrandCode& lhs, const T& rhs) {
    return std::string_view(lhs.chars) <=> std::string_view(rhs);
  }

  std::array<char, N> chars{};
};

}  // namespace

const char* g_brand_for_testing = nullptr;

// Global functions -----------------------------------------------------------

#if BUILDFLAG(IS_WIN)

bool GetBrand(std::string* brand) {
  if (g_brand_for_testing) {
    brand->assign(g_brand_for_testing);
    return true;
  }

  // Cache brand code value, since it is queried a lot and registry queries are
  // slow enough to actually affect top-level metrics like
  // Omnibox.CharTypedToRepaintLatency.
  static const base::NoDestructor<std::optional<std::string>> brand_code(
      []() -> std::optional<std::string> {
        std::wstring brandw;
        if (!GoogleUpdateSettings::GetBrand(&brandw)) {
          return std::nullopt;
        }
        return base::WideToASCII(brandw);
      }());
  if (!brand_code->has_value()) {
    return false;
  }
  brand->assign(**brand_code);
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  std::wstring brandw;
  bool ret = GoogleUpdateSettings::GetReactivationBrand(&brandw);
  if (ret) {
    brand->assign(base::WideToASCII(brandw));
  }
  return ret;
}

#elif !BUILDFLAG(IS_MAC)

bool GetBrand(std::string* brand) {
  if (g_brand_for_testing) {
    brand->assign(g_brand_for_testing);
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  brand->assign(google_brand::chromeos::GetBrand());
#else
  brand->clear();
#endif
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

#endif

bool GetRlzBrand(std::string* brand) {
#if BUILDFLAG(IS_CHROMEOS)
  brand->assign(google_brand::chromeos::GetRlzBrand());
  return true;
#else
  return GetBrand(brand);
#endif
}

bool IsOrganic(const std::string& brand) {
  if (brand.empty()) {
    // An empty brand string is considered the same as GGLS, which is organic.
    // On Mac, channels other than stable never have a brand code. Linux,
    // FreeBSD, and OpenBSD never have a brand code. Such installs are always
    // organic.
    return true;
  }

  static constexpr auto kOrganicBrands =
      base::MakeFixedFlatSet<FixedBrandCode<4>>(
          {"CHCA", "CHCB", "CHCG", "CHCH", "CHCI", "CHCJ", "CHCK", "CHCL",
           "CHFO", "CHFT", "CHHS", "CHHM", "CHMA", "CHMB", "CHME", "CHMF",
           "CHMG", "CHMH", "CHMI", "CHMQ", "CHMV", "CHNB", "CHNC", "CHNG",
           "CHNH", "CHNI", "CHOA", "CHOB", "CHOC", "CHON", "CHOO", "CHOP",
           "CHOQ", "CHOR", "CHOS", "CHOT", "CHOU", "CHOX", "CHOY", "CHOZ",
           "CHPD", "CHPE", "CHPF", "CHPG", "ECBA", "ECBB", "ECDA", "ECDB",
           "ECSA", "ECSB", "ECVA", "ECVB", "ECWA", "ECWB", "ECWC", "ECWD",
           "ECWE", "ECWF", "EUBB", "EUBC", "GCEL", "GGLA", "GGLS"});
  if (kOrganicBrands.contains(brand)) {
    return true;
  }

  // The Chrome enterprise brand code is the only GGR* brand to be non-organic.
  if (brand == "GGRV") {
    return false;
  }

  return base::StartsWith(brand, "EUB", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "EUC", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "GGR", base::CompareCase::SENSITIVE);
}

bool IsOrganicFirstRun(const std::string& brand) {
  if (brand.empty()) {
    // An empty brand string is the same as GGLS, which is organic.
    return true;
  }

  return base::StartsWith(brand, "GG", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "EU", base::CompareCase::SENSITIVE);
}

bool IsInternetCafeBrandCode(const std::string& brand) {
  static constexpr auto kBrands = base::MakeFixedFlatSet<FixedBrandCode<4>>(
      {"CHIQ", "CHSG", "HLJY", "IDCM", "NTMO", "OOBA", "OOBB", "OOBC", "OOBD",
       "OOBE", "OOBF", "OOBG", "OOBH", "OOBI", "OOBJ"});
  return kBrands.contains(brand);
}

bool IsEnterprise(const std::string& brand) {
  // GCEL is the only GCE* code that is actually organic.
  if (brand == "GCEL") {
    return false;
  }
  static constexpr auto kEnterpriseBrands =
      base::MakeFixedFlatSet<FixedBrandCode<3>>({
          // CBE brand codes.
          "GCE",
          "GCF",
          "GCG",
          "GCH",
          "GCO",
          "GCP",
          "GCQ",
          "GCS",
          // CBE+CBCM brand codes.
          "GCC",
          "GCK",
          "GCL",
          "GCM",
          "GCT",
          "GCU",
          "GCV",
          "GCW",
      });
  return brand == "GGRV" ||
         kEnterpriseBrands.contains(std::string_view(brand).substr(0, 3));
}

// BrandForTesting ------------------------------------------------------------

BrandForTesting::BrandForTesting(const std::string& brand) : brand_(brand) {
  CHECK(g_brand_for_testing == nullptr, base::NotFatalUntil::M161);
  g_brand_for_testing = brand_.c_str();
}

BrandForTesting::~BrandForTesting() {
  g_brand_for_testing = nullptr;
}

}  // namespace google_brand
