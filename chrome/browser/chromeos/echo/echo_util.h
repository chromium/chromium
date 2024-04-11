// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ECHO_ECHO_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ECHO_ECHO_UTIL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

// TODO(http://b/333583704): Revert CL which added this util after migration.
namespace chromeos::echo_util {

using GetOobeTimestampCallback = base::OnceCallback<void(
    base::expected</*value=*/std::string, /*error=*/std::string>
        oobe_timestamp_or_error)>;

// Asynchronously returns the OOBE timestamp corresponding to the time of device
// registration. For backwards compatibility:
// * The OOBE timestamp is a "y-M-d" formatted GMT date string.
// * An empty string is returned if OOBE timestamp is unavailable.
// * An error is returned in Lacros if the required CrosAPI is unavailable.
void GetOobeTimestamp(GetOobeTimestampCallback callback);

}  // namespace chromeos::echo_util

#endif  // CHROME_BROWSER_CHROMEOS_ECHO_ECHO_UTIL_H_
