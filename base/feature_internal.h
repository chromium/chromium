// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BASE_FEATURE_INTERNAL_H_
#define BASE_FEATURE_INTERNAL_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/containers/span.h"

namespace base::internal {

// Secret handshake to (try to) ensure all places that construct a base::Feature
// go through the helper `BASE_FEATURE()` and `BASE_RUNTIME_MUTABLE_FEATURE()`
// macros, below.
enum class FeatureMacroHandshake { kSecret };

namespace feature_state_masks {

enum : uint32_t {
  kRuntimeMutabilityMask = 1 << 18,
  // LINT.IfChange(FeatureDefaultState)
  kFeatureDefaultStateShift = 19,
  // LINT.ThenChange(feature.rs:FeatureDefaultState)
};

}  // namespace feature_state_masks

constexpr uint32_t MakeFeatureStateCacheInitialValue(bool is_runtime_mutable,
                                                     uint32_t default_state) {
  return (is_runtime_mutable ? feature_state_masks::kRuntimeMutabilityMask
                             : 0u) |
         (default_state << feature_state_masks::kFeatureDefaultStateShift);
}

// Returns true if all country codes in the given span are valid in a simple
// sense: A valid country code consists of exactly two lowercase ASCII letters.
consteval bool AreCountryCodesValid(
    base::span<const std::string_view> countries) {
  // This is a plain loop instead of `std::ranges::all_of()` because checking
  // the constraints of that costs several milliseconds of compile time in
  // every translation unit that includes this file.
  auto valid_letter = [](char c) { return c >= 'a' && c <= 'z'; };
  for (std::string_view code : countries) {
    if (code.size() != 2 || !valid_letter(code[0]) || !valid_letter(code[1])) {
      return false;
    }
  }
  return true;
}

// Packs a variadic list of country codes into a std::array and asserts
// statically that the list is non-empty.
// Not inlined to allow more descriptive error messages.
template <typename... Args>
consteval auto MakeCountryCodeStorage(Args... args) {
  static_assert(sizeof...(Args) > 0, "The country list must be non-empty");
  return std::array<std::string_view, sizeof...(Args)>{
      std::string_view(args)...};
}

}  // namespace base::internal

// Puts immutable feature descriptors into a RELRO ELF section. The .cr_features
// suffix lets SuperSize distinguish feature flags from mutable constants.
// __attribute__ is used rather than [[gnu::section]] because C++ attribute
// syntax cannot appear after declaration specifiers (e.g.,
// `static BASE_FEATURE(...)`).
// LINT.IfChange(FeatureSection)
#if defined(__ELF__)
#define BASE_FEATURE_SECTION \
  __attribute__((section(".data.rel.ro..cr_features")))
#else
#define BASE_FEATURE_SECTION
#endif
// LINT.ThenChange(feature.rs:FeatureSection)

// Groups writable feature state on Windows to limit the pages made private by
// cache writes. lld-link merges `.data$*` sections into `.data`, ordered by
// suffix.
// LINT.IfChange(FeatureStateSection)
#if defined(_WIN32)
#define BASE_FEATURE_STATE_SECTION \
  __attribute__((section(".data$cr_feature_states")))
#else
#define BASE_FEATURE_STATE_SECTION
#endif
// LINT.ThenChange(feature.rs:FeatureStateSection)

namespace base::internal {

// The state storage for one feature. The lambda Tag gives each feature distinct
// state storage. A variable template keeps BASE_FEATURE() a single declaration,
// so prefixes such as static and COMPONENT_EXPORT() still apply to the feature.
template <typename Tag, uint32_t initial_value>
BASE_FEATURE_STATE_SECTION inline constinit std::atomic<uint32_t>
    feature_state_storage{initial_value};

}  // namespace base::internal

// Returns a pointer to a new state storage object for a feature.
#define BASE_FEATURE_INTERNAL_STATE(is_runtime_mutable, default_state,  \
                                    state_predicate)                    \
  ([]<base::FeatureState kDefaultState>() {                             \
    static_assert(state_predicate(kDefaultState));                      \
    return &base::internal::feature_state_storage<                      \
        decltype([] {}),                                                \
        base::internal::MakeFeatureStateCacheInitialValue(              \
            is_runtime_mutable, static_cast<uint32_t>(kDefaultState))>; \
  }.template operator()<(default_state)>())

// Three-argument version of BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_3_ARGS(is_runtime_mutable, feature, name,  \
                                     default_state)                      \
  BASE_FEATURE_SECTION constinit const base::Feature feature(            \
      name,                                                              \
      BASE_FEATURE_INTERNAL_STATE(is_runtime_mutable, default_state,     \
                                  !base::IsCountrySpecificFeatureState), \
      base::internal::FeatureMacroHandshake::kSecret)

// Two-argument version of BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_2_ARGS(is_runtime_mutable, feature,        \
                                     default_state)                      \
  BASE_FEATURE_SECTION constinit const base::Feature feature(            \
      []() {                                                             \
        static_assert(#feature[0] == 'k');                               \
        return std::string_view(#feature).substr(1).data();              \
      }(),                                                               \
      BASE_FEATURE_INTERNAL_STATE(is_runtime_mutable, default_state,     \
                                  !base::IsCountrySpecificFeatureState), \
      base::internal::FeatureMacroHandshake::kSecret)

// Helper macro to deduce whether to use the 2 or 3 argument version of the
// BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_GET_FEATURE_MACRO(_1, _2, _3, NAME, ...) NAME

// Provides a definition for a country-restricted `kFeature` with
// `default_state` and a list of `countries`.
//
// Implementation note: `MakeCountryCodeStorage` is used to guarantee static
// storage duration.
#define BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(feature, default_state, ...) \
  BASE_FEATURE_SECTION constinit const base::FeatureWithCountryRestriction  \
      feature(                                                              \
          []() {                                                            \
            static_assert(#feature[0] == 'k');                              \
            return std::string_view(#feature).substr(1).data();             \
          }(),                                                              \
          BASE_FEATURE_INTERNAL_STATE(false, default_state,                 \
                                      base::IsCountrySpecificFeatureState), \
          []() {                                                            \
            static constexpr auto countries =                               \
                base::internal::MakeCountryCodeStorage(__VA_ARGS__);        \
            static_assert(base::internal::AreCountryCodesValid(countries),  \
                          "All country parameters must consist of two "     \
                          "characters between a and z");                    \
            return base::span(countries);                                   \
          }(),                                                              \
          base::internal::FeatureMacroHandshake::kSecret)

#define BASE_DECLARE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeature) \
  extern constinit const base::FeatureWithCountryRestriction kFeature

// Five-argument version of BASE_FEATURE_PARAM macro.
#define BASE_FEATURE_PARAM_INTERNAL_5_ARGS(T, feature_object_name, feature, \
                                           name, default_value)             \
  namespace field_trial_params_internal {                                   \
  T GetFeatureParamWithCacheFor##feature_object_name(                       \
      const base::FeatureParam<T>* feature_param) {                         \
    static const typename base::internal::FeatureParamTraits<               \
        T>::CacheStorageType storage =                                      \
        base::internal::FeatureParamTraits<T>::ToCacheStorageType(          \
            feature_param->GetWithoutCache());                              \
    return base::internal::FeatureParamTraits<T>::FromCacheStorageType(     \
        storage);                                                           \
  }                                                                         \
  } /* namespace field_trial_params_internal */                             \
  constinit const base::FeatureParam<T> feature_object_name(                \
      feature, name, default_value,                                         \
      &field_trial_params_internal::                                        \
          GetFeatureParamWithCacheFor##feature_object_name)

// Four-argument version of BASE_FEATURE_PARAM macro.
#define BASE_FEATURE_PARAM_INTERNAL_4_ARGS(T, feature_object_name, feature, \
                                           default_value)                   \
  BASE_FEATURE_PARAM_INTERNAL_5_ARGS(                                       \
      T, feature_object_name, feature,                                      \
      []() {                                                                \
        static_assert(#feature_object_name[0] == 'k');                      \
        return std::string_view(#feature_object_name).substr(1).data();     \
      }(),                                                                  \
      default_value)

// Helper macro to deduce the whether to use the 4 or 5 argument version of
// the BASE_FEATURE_PARAM macro.
#define BASE_FEATURE_INTERNAL_GET_FEATURE_PARAM_MACRO(_1, _2, _3, _4, _5, \
                                                      NAME, ...)          \
  NAME

// Six-argument version of BASE_FEATURE_ENUM_PARAM macro.
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS(                   \
    T, feature_object_name, feature, name, default_value, options) \
  namespace field_trial_params_internal {                          \
  T GetFeatureParamWithCacheFor##feature_object_name(              \
      const base::FeatureParam<T>* feature_param) {                \
    static const T param = feature_param->GetWithoutCache();       \
    return param;                                                  \
  }                                                                \
  } /* field_trial_params_internal */                              \
  constinit const base::FeatureParam<T> feature_object_name(       \
      feature, name, default_value, options,                       \
      &field_trial_params_internal::                               \
          GetFeatureParamWithCacheFor##feature_object_name)

// Five-argument version of BASE_FEATURE_ENUM_PARAM macro.
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_5_ARGS(                        \
    T, feature_object_name, feature, default_value, options)            \
  BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS(                              \
      T, feature_object_name, feature,                                  \
      []() {                                                            \
        static_assert(#feature_object_name[0] == 'k');                  \
        return std::string_view(#feature_object_name).substr(1).data(); \
      }(),                                                              \
      default_value, options)

// Helper macro to deduce the whether to use the 5 or 6 argument version of
// the BASE_FEATURE_ENUM_PARAM macro.
#define BASE_FEATURE_INTERNAL_GET_FEATURE_ENUM_PARAM_MACRO(_1, _2, _3, _4, _5, \
                                                           _6, NAME, ...)      \
  NAME

#endif  // BASE_FEATURE_INTERNAL_H_
