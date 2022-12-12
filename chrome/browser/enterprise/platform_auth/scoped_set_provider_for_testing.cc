// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"

#include <utility>

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

namespace enterprise_auth {

ScopedSetProviderForTesting::ScopedSetProviderForTesting(
    std::unique_ptr<PlatformAuthProvider> provider)
    : previous_(PlatformAuthProviderManager::GetInstance()
                    .SetProviderForTesting(  // IN-TEST
                        std::move(provider))) {}

ScopedSetProviderForTesting::~ScopedSetProviderForTesting() {
  PlatformAuthProviderManager::GetInstance().SetProviderForTesting(  // IN-TEST
      std::move(previous_));
}

}  // namespace enterprise_auth
