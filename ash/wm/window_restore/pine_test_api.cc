// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_test_api.h"

namespace ash {

PineContentsViewTestApi::PineContentsViewTestApi(
    const PineContentsView* pine_contents_view)
    : pine_contents_view_(pine_contents_view) {}

PineContentsViewTestApi::~PineContentsViewTestApi() = default;

PineItemsOverflowViewTestApi::PineItemsOverflowViewTestApi(
    const PineItemsOverflowView* overflow_view)
    : overflow_view_(overflow_view) {}

PineItemsOverflowViewTestApi::~PineItemsOverflowViewTestApi() = default;

}  // namespace ash
