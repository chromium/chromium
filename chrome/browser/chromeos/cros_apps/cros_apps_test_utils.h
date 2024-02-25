// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TEST_UTILS_H_

#include "content/public/test/browser_test_utils.h"

// This method checks the given identifier is defined in `wc_or_rfh`'s
// JavaScript.
[[nodiscard]] content::EvalJsResult IsIdentifierDefined(
    const content::ToRenderFrameHost& wc_or_rfh,
    const char* identifier);

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TEST_UTILS_H_
