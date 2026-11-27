// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/fixed_flat_set.h"

#include <array>
#include <compare>
#include <span>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"

// Constructing a fixed flat set with duplicate keys should not compile.
auto kDuplicates = base::MakeFixedFlatSet<int>({1, 1, 2, 3});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Constructing a fixed flat set with the sorted_unique tag but unsorted keys
// should not compile.
auto kNotActuallySorted = base::MakeFixedFlatSet<int>(base::sorted_unique, {3, 2, 1});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Constructing a fixed flat set with the sorted_unique tag but duplicate keys
// should not compile.
auto kSortedButDuplicates = base::MakeFixedFlatSet<int>(base::sorted_unique, {1, 1, 2});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Duplicate keys in a std::array should not compile.
constexpr std::array<int, 2> kDuplicateArray = {1, 1};
auto kDuplicatesFromArray = base::MakeFixedFlatSet<int>(kDuplicateArray);  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Duplicate keys produced by projecting a fixed_flat_map should not compile.
constexpr auto kMapWithDupValues =
    base::MakeFixedFlatMap<int, int>({{1, 10}, {2, 10}});
auto kDuplicatesFromProjection = base::MakeFixedFlatSet<int>(  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}
    kMapWithDupValues, {}, [](const auto& entry) { return entry.second; });

// Non-owning ranges are rejected by the container overload.
void NonOwningRanges(base::span<const int, 2> base_span,
                     std::span<const int, 2> std_span) {
  (void)base::MakeFixedFlatSet<int>(base_span);  // expected-error {{no matching function for call to 'MakeFixedFlatSet'}}
  (void)base::MakeFixedFlatSet<int>(std_span);   // expected-error {{no matching function for call to 'MakeFixedFlatSet'}}
}

// Narrowing conversions are rejected.
constexpr std::array<double, 2> kDoubles = {1.5, 2.5};
auto kNarrowing = base::MakeFixedFlatSet<int>(kDoubles);  // expected-error {{no matching function for call to 'MakeFixedFlatSet'}}

// Explicit constructors are rejected.
struct ExplicitInt {
  explicit constexpr ExplicitInt(int v) : value(v) {}
  auto operator<=>(const ExplicitInt&) const = default;
  int value;
};
constexpr std::array<int, 2> kInts = {2, 1};
auto kExplicitCtor = base::MakeFixedFlatSet<ExplicitInt>(kInts);  // expected-error {{no matching function for call to 'MakeFixedFlatSet'}}
