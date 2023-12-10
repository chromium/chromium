// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_switches.h"

#include <optional>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace {
std::optional<size_t> GetSizeTValueIfSetByCommandLineFlag(
    const std::string& switch_key) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (size_t value;
      command_line->HasSwitch(switch_key) &&
      base::StringToSizeT(command_line->GetSwitchValueASCII(switch_key),
                          &value)) {
    return value;
  }
  return std::nullopt;
}
}  // namespace

namespace bound_session_credentials {

// Used to add artificial delay to the cookie rotation request. It expects as
// value a number representing the delay in milliseconds.
const char kCookieRotationDelay[] = "bound-session-cookie-rotation-delay";

// Used to simulate the cookie rotation network request response.
// It expects as a value a number representing the enum value of
// `BoundSessionRefreshCookieFetcher::Result`.
// Note: This should be used to simulate error cases not success. If success `0`
// is used, bound cookies won't be set.
const char kCookieRotationResult[] = "bound-session-cookie-rotation-result";

std::optional<base::TimeDelta> GetCookieRotationDelayIfSetByCommandLine() {
  std::optional<size_t> value =
      GetSizeTValueIfSetByCommandLineFlag(kCookieRotationDelay);
  if (value) {
    return base::Milliseconds(*value);
  }
  return std::nullopt;
}

std::optional<BoundSessionRefreshCookieFetcher::Result>
GetCookieRotationResultIfSetByCommandLine() {
  std::optional<size_t> value =
      GetSizeTValueIfSetByCommandLineFlag(kCookieRotationResult);
  if (!value ||
      value > static_cast<int>(
                  BoundSessionRefreshCookieFetcher::Result::kMaxValue)) {
    return std::nullopt;
  }

  return static_cast<BoundSessionRefreshCookieFetcher::Result>(*value);
}

}  // namespace bound_session_credentials
