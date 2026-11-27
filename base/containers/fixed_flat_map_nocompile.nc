// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/fixed_flat_map.h"

#include <array>
#include <compare>
#include <span>
#include <utility>

#include "base/containers/span.h"

// Constructing a fixed flat map with duplicate keys should not compile.
auto kDuplicates = base::MakeFixedFlatMap<int, int>(  // expected-error-re {{call to consteval function 'base::MakeFixedFlatMap<{{.+}}>' is not a constant expression}}
    {{1, 0}, {1, 0}, {2, 0}, {3, 0}});

// Duplicate keys in a std::array should not compile.
constexpr auto kDuplicateArray =
    std::to_array<std::pair<int, int>>({{1, 0}, {1, 2}});
auto kDuplicatesFromArray = base::MakeFixedFlatMap<int, int>(kDuplicateArray);  // expected-error-re {{call to consteval function 'base::MakeFixedFlatMap<{{.+}}>' is not a constant expression}}

// Duplicate keys produced by projecting a fixed_flat_map should not compile.
constexpr auto kNonInjectiveMap =
    base::MakeFixedFlatMap<int, int>({{1, 10}, {2, 10}});
auto kDuplicatesFromProjection = base::MakeFixedFlatMap<int, int>(  // expected-error-re {{call to consteval function 'base::MakeFixedFlatMap<{{.+}}>' is not a constant expression}}
    kNonInjectiveMap, {},
    [](const auto& entry) { return std::pair(entry.second, entry.first); });

// Non-owning ranges are rejected by the container overload.
void NonOwningRanges(base::span<const std::pair<int, int>, 2> base_span,
                     std::span<const std::pair<int, int>, 2> std_span) {
  (void)base::MakeFixedFlatMap<int, int>(base_span);  // expected-error {{no matching function for call to 'MakeFixedFlatMap'}}
  (void)base::MakeFixedFlatMap<int, int>(std_span);   // expected-error {{no matching function for call to 'MakeFixedFlatMap'}}
}

// Narrowing conversions are rejected for both keys and mapped values.
constexpr auto kDoubleKeyPairs =
    std::to_array<std::pair<double, int>>({{1.5, 10}, {2.5, 20}});
auto kNarrowingKey = base::MakeFixedFlatMap<int, int>(kDoubleKeyPairs);  // expected-error {{no matching function for call to 'MakeFixedFlatMap'}}
constexpr auto kDoubleValuePairs =
    std::to_array<std::pair<int, double>>({{1, 1.5}, {2, 2.5}});
auto kNarrowingValue = base::MakeFixedFlatMap<int, int>(kDoubleValuePairs);  // expected-error {{no matching function for call to 'MakeFixedFlatMap'}}

// Explicit constructors are rejected.
struct ExplicitInt {
  explicit constexpr ExplicitInt(int v) : value(v) {}
  auto operator<=>(const ExplicitInt&) const = default;
  int value;
};
constexpr auto kIntPairs = std::to_array<std::pair<int, int>>({{2, 20}, {1, 10}});
auto kExplicitCtor = base::MakeFixedFlatMap<ExplicitInt, int>(kIntPairs);  // expected-error {{no matching function for call to 'MakeFixedFlatMap'}}
