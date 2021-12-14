// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {

// Get the resource ID of the input overlay JSON file by the associated package
// name.
absl::optional<int> GetInputOverlayResourceId(const std::string& package_name);

// Parse Json to different types of actions.
absl::optional<std::vector<std::unique_ptr<input_overlay::Action>>>
ParseJsonToActions(aura::Window* window, const base::Value& root);

// Parse Json to different types of positions.
absl::optional<std::vector<std::unique_ptr<input_overlay::Position>>>
ParseLocation(const base::Value& position);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_
