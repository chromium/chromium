// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"

namespace enterprise_auth {

MockPlatformAuthProvider::MockPlatformAuthProvider() = default;

MockPlatformAuthProvider::~MockPlatformAuthProvider() {
  Die();
}

}  // namespace enterprise_auth
