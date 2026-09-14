// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_SAPISID_GENERATOR_H_
#define CHROME_BROWSER_LENS_LENS_SAPISID_GENERATOR_H_

#include <optional>
#include <string>

#include "base/time/time.h"

namespace lens {

// Generates the SAPISIDHASH Authorization header value using the Optimization
// Guide library. Returns std::nullopt if the library is unavailable or hash
// generation fails.
std::optional<std::string> GenerateSapisidHash(
    const std::string& email,
    const std::string& sapisid_cookie,
    const std::string& origin,
    base::Time timestamp);

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_SAPISID_GENERATOR_H_
