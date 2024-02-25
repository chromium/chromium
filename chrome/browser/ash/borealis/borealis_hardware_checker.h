// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_HARDWARE_CHECKER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_HARDWARE_CHECKER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace borealis {

// Checks the current hardware configuration to determine if it meets the
// minimum specifications.
//
// Invokes `callback` with "true" if it does, "false" if it doesn't, or if
// we fail to determine either way.
void HasSufficientHardware(base::OnceCallback<void(bool)> callback);

// Used in tests to fake the CPU, since base::CPU doesn't support faking.
// Make sure *`cpu_brand` outlives the actual check against it.
void SetCpuForTesting(std::string* cpu_brand);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_HARDWARE_CHECKER_H_
