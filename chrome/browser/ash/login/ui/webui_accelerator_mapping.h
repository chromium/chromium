// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_WEBUI_ACCELERATOR_MAPPING_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_WEBUI_ACCELERATOR_MAPPING_H_

#include <string>

#include "ash/public/cpp/login_accelerators.h"

namespace ash {

std::string MapToWebUIAccelerator(LoginAcceleratorAction action);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_WEBUI_ACCELERATOR_MAPPING_H_
