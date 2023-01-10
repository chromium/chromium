// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/platform_keys/chaps_util.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace platform_keys {

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
          std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>>,
          chromeos::platform_keys::Status> {
 public:
  const std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>>&
  token_ids();
  chromeos::platform_keys::Status status();
};

// Supports waiting for the result of the PlatformKeysService::GenerateKey*
// function family.
class GenerateKeyExecutionWaiter
    : public base::test::TestFuture<std::string,
                                    chromeos::platform_keys::Status> {
 public:
  const std::string& public_key_spki_der();
  chromeos::platform_keys::Status status();

  base::OnceCallback<void(const std::string&, chromeos::platform_keys::Status)>
  GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::Sign* function
// family.
class SignExecutionWaiter
    : public base::test::TestFuture<std::string,
                                    chromeos::platform_keys::Status> {
 public:
  const std::string& signature();
  chromeos::platform_keys::Status status();

  base::OnceCallback<void(const std::string&, chromeos::platform_keys::Status)>
  GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public base::test::TestFuture<std::unique_ptr<net::CertificateList>,
                                    chromeos::platform_keys::Status> {
 public:
  const net::CertificateList& matches();
  chromeos::platform_keys::Status status();
};

// Supports waiting for the result of the
// PlatformKeysService::SetAttributeForKey.
using SetAttributeForKeyExecutionWaiter = StatusWaiter;

// Supports waiting for the result of the
// PlatformKeysService::GetAttributeForKey.
class GetAttributeForKeyExecutionWaiter
    : public base::test::TestFuture<absl::optional<std::string>,
                                    chromeos::platform_keys::Status> {
 public:
  const absl::optional<std::string>& attribute_value();
  chromeos::platform_keys::Status status();

  base::OnceCallback<void(const absl::optional<std::string>&,
                          chromeos::platform_keys::Status)>
  GetCallback();
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
using RemoveKeyExecutionWaiter = StatusWaiter;

class GetAllKeysExecutionWaiter
    : public base::test::TestFuture<std::vector<std::string>,
                                    chromeos::platform_keys::Status> {
 public:
  const std::vector<std::string>& public_keys();
  chromeos::platform_keys::Status status();
};

class IsKeyOnTokenExecutionWaiter
    : public base::test::TestFuture<absl::optional<bool>,
                                    chromeos::platform_keys::Status> {
 public:
  absl::optional<bool> on_slot();
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

// A fake implementation of ChapsUtil which actually just generates a key pair
// through NSS.
class FakeChapsUtil : public ChapsUtil {
 public:
  using OnKeyGenerated = base::RepeatingCallback<void(const std::string& spki)>;

  explicit FakeChapsUtil(OnKeyGenerated on_key_generated);
  ~FakeChapsUtil() override;

  bool GenerateSoftwareBackedRSAKey(
      PK11SlotInfo* slot,
      uint16_t num_bits,
      crypto::ScopedSECKEYPublicKey* out_public_key,
      crypto::ScopedSECKEYPrivateKey* out_private_key) override;

 private:
  OnKeyGenerated on_key_generated_;
};

// While an instance of this class exists, ChapsUtil::Create() will return
// instances of FakeChapsUtil (see above). Only one instance of this class
// should exist at a time.
class ScopedChapsUtilOverride {
 public:
  // Sets up ChapsUtil to return instances of FakeChapsUtil (see above) from
  // ChapsUtil::Create().
  ScopedChapsUtilOverride();
  // Reverts the changes performed by the constructor.
  ~ScopedChapsUtilOverride();

  // Returns all der-encoded SPKIs that were generated through ChapsUtil
  // instances returned from ChapsUtil::Create() while this override was active,
  // i.e. since this instances has been constructed.
  const std::vector<std::string>& generated_key_spkis() {
    return generated_key_spkis_;
  }

 private:
  std::unique_ptr<ChapsUtil> CreateChapsUtil();

  // Called when a FakeChapsUtil instance created by CreateChapsUtil generates a
  // key pair.
  void OnKeyGenerated(const std::string& spki);

  // Tracks key pairs that were generated through FakeChapsUtil instances
  // created by CreateChapsUtil().
  std::vector<std::string> generated_key_spkis_;

  base::WeakPtrFactory<ScopedChapsUtilOverride> weak_ptr_factory_{this};
};

}  // namespace test_util
}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
