// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_

#include <optional>
#include <string>

namespace arc::input_overlay {

// Get the resource ID of the input overlay JSON file by the associated package
// name.
std::optional<int> GetInputOverlayResourceId(const std::string& package_name);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_INPUT_OVERLAY_RESOURCES_UTIL_H_
