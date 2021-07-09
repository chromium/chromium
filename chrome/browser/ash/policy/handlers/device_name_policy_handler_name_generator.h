// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_

#include <string>

namespace policy {

// Uses template to build a hostname. Returns valid hostname (after parameter
// substitution) or empty string, if substitution result is not a valid
// hostname.
std::string FormatHostname(const std::string& name_template,
                           const std::string& asset_id,
                           const std::string& serial,
                           const std::string& mac,
                           const std::string& machine_name,
                           const std::string& location);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_
