// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/scoped_policy_update.h"

#include <utility>

namespace ash {

ScopedUserPolicyUpdate::ScopedUserPolicyUpdate(
    policy::UserPolicyBuilder* policy_builder,
    base::OnceClosure callback)
    : policy_builder_(policy_builder), callback_(std::move(callback)) {}

ScopedUserPolicyUpdate::~ScopedUserPolicyUpdate() {
  std::move(callback_).Run();
}

ScopedDevicePolicyUpdate::ScopedDevicePolicyUpdate(
    policy::DevicePolicyBuilder* policy_builder,
    base::OnceClosure callback)
    : policy_builder_(policy_builder), callback_(std::move(callback)) {}

ScopedDevicePolicyUpdate::~ScopedDevicePolicyUpdate() {
  std::move(callback_).Run();
}

}  // namespace ash
