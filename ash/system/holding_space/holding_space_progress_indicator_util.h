// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_UTIL_H_

#include <memory>

namespace ash {

class HoldingSpaceController;
class HoldingSpaceItem;
class ProgressIndicator;

namespace holding_space_util {

// Returns a `ProgressIndicator` instance which paints indication of progress
// for all holding space items in the model attached to the specified
// `controller`.
std::unique_ptr<ProgressIndicator> CreateProgressIndicatorForController(
    HoldingSpaceController* controller);

// Returns a `ProgressIndicator` instance which paints indication of progress
// for the specified holding space `item`.
std::unique_ptr<ProgressIndicator> CreateProgressIndicatorForItem(
    const HoldingSpaceItem* item);

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_UTIL_H_
