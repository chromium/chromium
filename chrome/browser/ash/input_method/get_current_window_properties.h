// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_GET_CURRENT_WINDOW_PROPERTIES_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_GET_CURRENT_WINDOW_PROPERTIES_H_

#include <optional>

#include "url/gurl.h"

namespace ash {
namespace input_method {

struct WindowProperties {
  std::string app_id;
  std::string arc_package_name;
};

std::optional<GURL> GetFocusedTabUrl();

WindowProperties GetFocusedWindowProperties();

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_GET_CURRENT_WINDOW_PROPERTIES_H_
