// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_IDENTITY_UTILS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_IDENTITY_UTILS_H_

#include "chrome/browser/profiles/profile.h"

namespace ash::input_method {

absl::optional<std::string> GetSignedInUserEmailFromProfile(Profile* profile);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_IDENTITY_UTILS_H_
