// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace platform_keys {

class PlatformKeysService;

namespace test_util {

// A helper that waits until execution of an asynchronous PlatformKeysService
// operation that only passes a |status| field to the callback.
class StatusWaiter : public base::test::TestFuture<Status> {
 public:
  Status status();
};

// Supports waiting for the result of PlatformKeysService::GetTokens.
class GetTokensExecutionWaiter
    : public base::test::TestFuture<std::unique_ptr<std::vector<TokenId>>,
                                    Status> {
 public:
  const std::unique_ptr<std::vector<TokenId>>& token_ids();
  Status status();
};

// Supports waiting for the result of the PlatformKeysService::GenerateKey*
// function family.
class GenerateKeyExecutionWaiter
    : public base::test::TestFuture<std::string, Status> {
 public:
  const std::string& public_key_spki_der();
  Status status();

  base::OnceCallback<void(const std::string&, Status)> GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::Sign* function
// family.
class SignExecutionWaiter : public base::test::TestFuture<std::string, Status> {
 public:
  const std::string& signature();
  Status status();

  base::OnceCallback<void(const std::string&, Status)> GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public base::test::TestFuture<std::unique_ptr<net::CertificateList>,
                                    Status> {
 public:
  const net::CertificateList& matches();
  Status status();
};

// Supports waiting for the result of the
// PlatformKeysService::SetAttributeForKey.
using SetAttributeForKeyExecutionWaiter = StatusWaiter;

// Supports waiting for the result of the
// PlatformKeysService::GetAttributeForKey.
class GetAttributeForKeyExecutionWaiter
    : public base::test::TestFuture<absl::optional<std::string>, Status> {
 public:
  const absl::optional<std::string>& attribute_value();
  Status status();

  base::OnceCallback<void(const absl::optional<std::string>&, Status)>
  GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
using RemoveKeyExecutionWaiter = StatusWaiter;

class GetAllKeysExecutionWaiter
    : public base::test::TestFuture<std::vector<std::string>, Status> {
 public:
  const std::vector<std::string>& public_keys();
  Status status();
};

class IsKeyOnTokenExecutionWaiter
    : public base::test::TestFuture<absl::optional<bool>, Status> {
 public:
  absl::optional<bool> on_slot();
  Status status();
};

class GetKeyLocationsExecutionWaiter
    : public base::test::TestFuture<std::vector<TokenId>, Status> {
 public:
  const std::vector<TokenId>& key_locations();
  Status status();

  base::OnceCallback<void(const std::vector<TokenId>&, Status)> GetCallback();
};

}  // namespace test_util
}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
