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

#endif  // BASE_FEATURE_INTERNAL_H_
