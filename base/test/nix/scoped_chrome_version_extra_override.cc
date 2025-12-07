// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/nix/scoped_chrome_version_extra_override.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/environment.h"
#include "base/notreached.h"
#include "base/strings/cstring_view.h"
#include "base/version_info/channel.h"
#include "base/version_info/nix/version_extra_utils.h"

namespace base::test {

namespace {

// Exchanges the value of the environment variable `name` with `new_value`;
// returning its previous value or null if it was not set. The variable is
// removed from the environment if `new_value` is null.
std::optional<std::string> ExchangeEnvironmentVariable(
    base::cstring_view name,
    std::optional<std::string> new_value) {
  auto environment = base::Environment::Create();
  std::optional<std::string> old_value = environment->GetVar(name);
  if (new_value) {
    environment->SetVar(name, *new_value);
  } else {
    environment->UnSetVar(name);
  }
  return old_value;
}

std::string GetVersionExtra(version_info::Channel channel,
                            bool is_extended_stable) {
  if (is_extended_stable) {
    CHECK_EQ(channel, version_info::Channel::STABLE);
    return "extended";
  }
  switch (channel) {
    case version_info::Channel::STABLE:
      return "stable";
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::DEV:
      return "unstable";
    case version_info::Channel::CANARY:
      return "canary";
    case version_info::Channel::UNKNOWN:
      return std::string();
  }
  NOTREACHED();
}

}  // namespace

ScopedChromeVersionExtraOverride::ScopedChromeVersionExtraOverride(
    version_info::Channel channel,
    bool is_extended_stable)
    : old_env_var_(ExchangeEnvironmentVariable(
          version_info::nix::kChromeVersionExtra,
          GetVersionExtra(channel, is_extended_stable))) {}

ScopedChromeVersionExtraOverride::~ScopedChromeVersionExtraOverride() {
  ExchangeEnvironmentVariable(version_info::nix::kChromeVersionExtra,
                              std::move(old_env_var_));
}

}  // namespace base::test
