// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BASE_FEATURE_INTERNAL_H_
#define BASE_FEATURE_INTERNAL_H_

#include <array>
#include <cstddef>

#include "base/containers/span.h"

namespace base::internal {

// Secret handshake to (try to) ensure all places that construct a base::Feature
// go through the helper `BASE_FEATURE()` macro above.
enum class FeatureMacroHandshake { kSecret };

// Storage class for feature name. This is needed so we store the feature name
// "MyFeature" instead of the feature identifier name "kMyFeature" in .rodata.
template <size_t N>
struct StringStorage {
  explicit constexpr StringStorage(base::span<const char, N + 1> feature) {
    static_assert(N > 2, "Feature name cannot be too short.");
    for (size_t i = 0; i < N; ++i) {
      storage[i] = feature[i + 1];
    }
  }

  std::array<char, N> storage;
};

// Deduce how much storage is needed for a given string literal. `feature`
// includes space for a NUL terminator; `StringStorage` also needs storage
// for the NUL terminator but drops the first character.
template <size_t N>
StringStorage(const char (&feature)[N]) -> StringStorage<N - 1>;

}  // namespace base::internal

// Three-argument version of BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_3_ARGS(feature, name, default_state) \
  constinit const base::Feature feature(                           \
      name, default_state, base::internal::FeatureMacroHandshake::kSecret)

// Two-argument version of BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_2_ARGS(feature, default_state)              \
  constinit const base::Feature feature(                                  \
      []() {                                                              \
        static_assert(#feature[0] == 'k');                                \
        static constexpr base::internal::StringStorage storage(#feature); \
        return storage.storage.data();                                    \
      }(),                                                                \
      default_state, base::internal::FeatureMacroHandshake::kSecret)

  // Helper macro to deduce the whether to use the 2 or 3 argument version of
  // the BASE_FEATURE macro.
#define BASE_FEATURE_INTERNAL_GET_FEATURE_MACRO(_1, _2, _3, NAME, ...) NAME

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
  } /* field_trial_params_internal */                                       \
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
        static constexpr base::internal::StringStorage storage(             \
            #feature_object_name);                                          \
        return storage.storage.data();                                      \
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
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_5_ARGS(                \
    T, feature_object_name, feature, default_value, options)    \
  BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS(                      \
      T, feature_object_name, feature,                          \
      []() {                                                    \
        static_assert(#feature_object_name[0] == 'k');          \
        static constexpr base::internal::StringStorage storage( \
            #feature_object_name);                              \
        return storage.storage.data();                          \
      }(),                                                      \
      default_value, options)

// Helper macro to deduce the whether to use the 5 or 6 argument version of
// the BASE_FEATURE_ENUM_PARAM macro.
#define BASE_FEATURE_INTERNAL_GET_FEATURE_ENUM_PARAM_MACRO(_1, _2, _3, _4, _5, \
                                                           _6, NAME, ...)      \
  NAME

#endif  // BASE_FEATURE_INTERNAL_H_
