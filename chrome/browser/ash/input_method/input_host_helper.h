// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_HOST_HELPER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_HOST_HELPER_H_

#include <string>

#include "ash/constants/app_types.h"

namespace ash {
namespace input_method {

struct InputAssociatedHost {
  // Type of app associated with this text field.
  ash::AppType app_type;
  // Key of app associated with this text field.
  std::string app_key;
};

void PopulateInputHost(InputAssociatedHost* host);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_HOST_HELPER_H_
