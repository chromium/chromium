// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

// Runs the rule check against contextual info.
bool IsAssistiveInputDisabled(const absl::optional<GURL>& current_url);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_H_
