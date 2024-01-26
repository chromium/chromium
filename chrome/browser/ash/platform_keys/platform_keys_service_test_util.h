// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

namespace ash::platform_keys {

class PlatformKeysService;

namespace test_util {

// A helper that waits until execution of an asynchronous PlatformKeysService
// operation that only passes a |status| field to the callback.
class StatusWaiter
    : public base::test::TestFuture<chromeos::platform_keys::Status> {
 public:
  chromeos::platform_keys::Status status();
};

// Supports waiting for the result of PlatformKeysService::GetTokens.
class GetTokensExecutionWaiter
    : public base::test::TestFuture<
          std::vector<chromeos::platform_keys::TokenId>,
          chromeos::platform_keys::Status> {
 public:
  const std::vector<chromeos::platform_keys::TokenId>& token_ids();
  chromeos::platform_keys::Status status();
};

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public base::test::TestFuture<std::unique_ptr<net::CertificateList>,
                                    chromeos::platform_keys::Status> {
 public:
  const net::CertificateList& matches();
  chromeos::platform_keys::Status status();
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
using RemoveKeyExecutionWaiter = StatusWaiter;

class IsKeyOnTokenExecutionWaiter
    : public base::test::TestFuture<std::optional<bool>,
                                    chromeos::platform_keys::Status> {
 public:
  std::optional<bool> on_slot();
  chromeos::platform_keys::Status status();
};

class GetKeyLocationsExecutionWaiter
    : public base::test::TestFuture<
          std::vector<chromeos::platform_keys::TokenId>,
          chromeos::platform_keys::Status> {
 public:
  const std::vector<chromeos::platform_keys::TokenId>& key_locations();
  chromeos::platform_keys::Status status();

  base::OnceCallback<void(const std::vector<chromeos::platform_keys::TokenId>&,
                          chromeos::platform_keys::Status)>
  GetCallback();
};

}  // namespace test_util
}  // namespace ash::platform_keys

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
