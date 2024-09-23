// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_
#define ASH_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_impl.h"
#include "ash/components/kcer/kcer_nss/kcer_token_impl_nss.h"
#include "ash/components/kcer/key_permissions.pb.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/cert_builder.h"

namespace kcer {

struct KeyAndCert {
  KeyAndCert(PublicKey key, scoped_refptr<const Cert> cert);
  KeyAndCert(KeyAndCert&&);
  KeyAndCert& operator=(KeyAndCert&&);
  ~KeyAndCert();

  PublicKey key;
  scoped_refptr<const Cert> cert;
};

//==============================================================================

// A helper class to work with tokens (that exist on the IO thread) from the UI
// thread.
class TokenHolder {
 public:
  // Creates a KcerToken of the type `token` and moves it to the IO thread. If
  // `initialize` then the KcerToken will be ready to process requests
  // immediately.
  TokenHolder(Token token,
              HighLevelChapsClient* chaps_client,
              bool initialize_token);
  TokenHolder(Token token,
              HighLevelChapsClient* chaps_client,
              bool initialize_token,
              crypto::ScopedPK11Slot nss_slot);
  ~TokenHolder();

  // If KcerToken was not initialized on construction, this method makes it
  // initialized. Can be used to simulate delayed initialization.
  void InitializeToken();
  // If KcerToken was not initialized on construction, this method simulates
  // initialization failure.
  void FailTokenInitialization();

  // Returns a weak pointer to the token that can be used to post requests for
  // it. The pointer should only be dereferenced on the IO thread.
  base::WeakPtr<internal::KcerToken> GetWeakPtr() { return weak_ptr_; }

  uint32_t GetSlotId();

 private:
  void Initialize(Token token,
                  HighLevelChapsClient* chaps_client,
                  bool initialize,
                  crypto::ScopedPK11Slot nss_slot);

  base::WeakPtr<internal::KcerToken> weak_ptr_;
  std::unique_ptr<internal::KcerTokenImplNss> io_token_;
  crypto::ScopedTestNSSDB nss_db_;
  crypto::ScopedPK11Slot nss_slot_;
  bool is_initialized_ = false;
};

//==============================================================================

// A test helper class that creates and initializes a Kcer instances with a user
// and device NSS slots. In the current implementation the HighLevelChapsClient
// is not configured, so certain functionality will not work (mainly PKCS#12
// import).
class TestKcerHolder {
 public:
  // nullptr can be passed for any/all slots to emulate that Kcer doesn't have
  // access to them.
  TestKcerHolder(PK11SlotInfo* user_slot, PK11SlotInfo* device_slot);
  ~TestKcerHolder();

  base::WeakPtr<Kcer> GetKcer();

 private:
  TokenHolder user_token_;
  TokenHolder device_token_;
  std::unique_ptr<kcer::internal::KcerImpl> kcer_;
};

//==============================================================================

// Compares two KerPermissions, returns true if they are equal.
bool ExpectKeyPermissionsEqual(const std::optional<chaps::KeyPermissions>& a,
                               const std::optional<chaps::KeyPermissions>& b);

// Verifies `signature` created with `signing_scheme` and the public key from
// `spki` for `data_to_sign`. By default (with `strict` == true) only returns
// true if the signature is correct. With `strict` == false, silently ignores
// schemes for which the verification is not implemented yet and also returns
// true for them. Returns false if signature is incorrect.
bool VerifySignature(SigningScheme signing_scheme,
                     PublicKeySpki spki,
                     DataToSign data_to_sign,
                     Signature signature,
                     bool strict = true);

// Returns |hash| prefixed with DER-encoded PKCS#1 DigestInfo with
// AlgorithmIdentifier=id-sha256.
// This is useful for testing Kcer::SignRsaPkcs1Raw which only
// appends PKCS#1 v1.5 padding before signing.
std::vector<uint8_t> PrependSHA256DigestInfo(base::span<const uint8_t> hash);

// Reads a file in the PEM format, decodes it, returns the content of the first
// PEM block in the DER format. Currently supports CERTIFICATE and PRIVATE KEY
// block types.
std::optional<std::vector<uint8_t>> ReadPemFileReturnDer(
    const base::FilePath& path);

// Can be used together with MakeCertBuilder().
std::unique_ptr<net::CertBuilder> MakeCertIssuer();

// Creates a certificate builder that can generate a self-signed certificate for
// the `public_key`. Requires an `issuer` that can be created using
// MakeCertIssuer().
std::unique_ptr<net::CertBuilder> MakeCertBuilder(
    net::CertBuilder* issuer,
    const std::vector<uint8_t>& public_key);

// Reads a file with the `file_name` from net::GetTestCertsDirectory()
// directory.
std::vector<uint8_t> ReadTestFile(const std::string& file_name);

// Reads the key and the cert from disk and imports them into Kcer.
base::expected<KeyAndCert, Error> ImportTestKeyAndCert(
    base::WeakPtr<Kcer> kcer,
    Token token,
    std::string_view key_filename,
    std::string_view cert_filename);

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_
