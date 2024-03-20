// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/fixed_flat_map.h"

// Constructing a fixed flat map with duplicate keys should not compile.
auto kDuplicates = base::MakeFixedFlatMap<int, int>(  // expected-error-re {{call to consteval function 'base::MakeFixedFlatMap<{{.+}}>' is not a constant expression}}
    {{1, 0}, {1, 0}, {2, 0}, {3, 0}});
