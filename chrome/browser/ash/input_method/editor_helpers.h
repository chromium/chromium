// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_HELPERS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_HELPERS_H_

#include <string>

#include "ui/gfx/range/range.h"

namespace ash::input_method {

std::string GetSystemLocale();

bool ShouldUseL10nStrings();

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_HELPERS_H_
