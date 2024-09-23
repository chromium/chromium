// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/fixed_flat_set.h"

// Constructing a fixed flat set with duplicate keys should not compile.
auto kDuplicates = base::MakeFixedFlatSet<int>({1, 1, 2, 3});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Constructing a fixed flat set with the sorted_unique tag but unsorted keys
// should not compile.
auto kNotActuallySorted = base::MakeFixedFlatSet<int>(base::sorted_unique, {3, 2, 1});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}

// Constructing a fixed flat set with the sorted_unique tag but duplicate keys
// should not compile.
auto kSortedButDuplicates = base::MakeFixedFlatSet<int>(base::sorted_unique, {1, 1, 2});  // expected-error-re {{call to consteval function 'base::MakeFixedFlatSet<{{.+}}>' is not a constant expression}}
