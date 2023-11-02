// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_UI_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_UI_UTILS_H_

#include <string>

#include "components/policy/core/browser/webui/json_generation.h"

namespace policy {
// Returns a JsonGenerationParams instance will all fields populated with
// platform specific data and `application_name`.
JsonGenerationParams GetChromeMetadataParams(
    const std::string& application_name);
}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_UI_UTILS_H_
