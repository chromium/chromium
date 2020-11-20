// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_test_util.h"

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

namespace chromeos {
namespace platform_keys {
namespace test_util {

GetTokensExecutionWaiter::GetTokensExecutionWaiter() = default;
GetTokensExecutionWaiter::~GetTokensExecutionWaiter() = default;

GenerateKeyExecutionWaiter::GenerateKeyExecutionWaiter() = default;
GenerateKeyExecutionWaiter::~GenerateKeyExecutionWaiter() = default;

SignExecutionWaiter::SignExecutionWaiter() = default;
SignExecutionWaiter::~SignExecutionWaiter() = default;

GetCertificatesExecutionWaiter::GetCertificatesExecutionWaiter() = default;
GetCertificatesExecutionWaiter::~GetCertificatesExecutionWaiter() = default;

SetAttributeForKeyExecutionWaiter::SetAttributeForKeyExecutionWaiter() =
    default;
SetAttributeForKeyExecutionWaiter::~SetAttributeForKeyExecutionWaiter() =
    default;

GetAttributeForKeyExecutionWaiter::GetAttributeForKeyExecutionWaiter() =
    default;
GetAttributeForKeyExecutionWaiter::~GetAttributeForKeyExecutionWaiter() =
    default;

RemoveKeyExecutionWaiter::RemoveKeyExecutionWaiter() = default;
RemoveKeyExecutionWaiter::~RemoveKeyExecutionWaiter() = default;

GetAllKeysExecutionWaiter::GetAllKeysExecutionWaiter() = default;
GetAllKeysExecutionWaiter::~GetAllKeysExecutionWaiter() = default;

IsKeyOnTokenExecutionWaiter::IsKeyOnTokenExecutionWaiter() = default;
IsKeyOnTokenExecutionWaiter::~IsKeyOnTokenExecutionWaiter() = default;

GetKeyLocationsExecutionWaiter::GetKeyLocationsExecutionWaiter() = default;
GetKeyLocationsExecutionWaiter::~GetKeyLocationsExecutionWaiter() = default;

}  // namespace test_util
}  // namespace platform_keys
}  // namespace chromeos
