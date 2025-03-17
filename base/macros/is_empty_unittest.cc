// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros/is_empty.h"

static_assert(BASE_IS_EMPTY() == 1);
static_assert(BASE_IS_EMPTY(a) == 0);
static_assert(BASE_IS_EMPTY(a, b) == 0);
static_assert(BASE_IS_EMPTY(a, b, c) == 0);

// Make sure that any args are expanded before emptiness is assessed.
#define EXPAND_EMPTY()
static_assert(BASE_IS_EMPTY(EXPAND_EMPTY()) == 1);
