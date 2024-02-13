// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_

#include <string>
#include <string_view>

namespace policy {

// Uses template to build a hostname. Returns valid hostname (after parameter
// substitution) or empty string, if substitution result is not a valid
// hostname.
std::string FormatHostname(std::string_view name_template,
                           std::string_view asset_id,
                           std::string_view serial,
                           std::string_view mac,
                           std::string_view machine_name,
                           std::string_view location);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_NAME_GENERATOR_H_
