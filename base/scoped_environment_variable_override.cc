// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_environment_variable_override.h"

#include "base/environment.h"

namespace base {

ScopedEnvironmentVariableOverride::ScopedEnvironmentVariableOverride(
    const std::string& variable_name,
    const std::string& value,
    bool unset_var)
    : environment_(Environment::Create()),
      variable_name_(variable_name),
      overridden_(false),
      old_value_(environment_->GetVar(variable_name)) {
  if (unset_var) {
    overridden_ = environment_->UnSetVar(variable_name);
  } else {
    overridden_ = environment_->SetVar(variable_name, value);
  }
}

ScopedEnvironmentVariableOverride::ScopedEnvironmentVariableOverride(
    const std::string& variable_name,
    const std::string& value)
    : ScopedEnvironmentVariableOverride(variable_name, value, false) {}

ScopedEnvironmentVariableOverride::ScopedEnvironmentVariableOverride(
    const std::string& variable_name)
    : ScopedEnvironmentVariableOverride(variable_name, "", true) {}

ScopedEnvironmentVariableOverride::ScopedEnvironmentVariableOverride(
    ScopedEnvironmentVariableOverride&&) = default;

ScopedEnvironmentVariableOverride& ScopedEnvironmentVariableOverride::operator=(
    ScopedEnvironmentVariableOverride&&) = default;

ScopedEnvironmentVariableOverride::~ScopedEnvironmentVariableOverride() {
  if (environment_ && overridden_) {
    if (old_value_.has_value()) {
      environment_->SetVar(variable_name_, old_value_.value());
    } else {
      environment_->UnSetVar(variable_name_);
    }
  }
}

}  // namespace base
