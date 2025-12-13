// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_libc_timezone_override.h"

#include "base/check.h"
#include "base/environment.h"

namespace base::test {
namespace {
constexpr char kTZ[] = "TZ";
}  // namespace

ScopedLibcTimezoneOverride::ScopedLibcTimezoneOverride(
    const std::string& timezone) {
  auto env = base::Environment::Create();
  old_value_ = env->GetVar(kTZ);
  CHECK(env->SetVar(kTZ, timezone));
  tzset();
}

ScopedLibcTimezoneOverride::~ScopedLibcTimezoneOverride() {
  auto env = base::Environment::Create();
  if (old_value_.has_value()) {
    CHECK(env->SetVar(kTZ, *old_value_));
  } else {
    CHECK(env->UnSetVar(kTZ));
  }
  tzset();
}

}  // namespace base::test
