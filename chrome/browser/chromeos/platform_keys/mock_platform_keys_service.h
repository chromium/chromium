// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
namespace platform_keys {

class MockPlatformKeysService : public PlatformKeysService {
 public:
  MockPlatformKeysService();
  MockPlatformKeysService(const MockPlatformKeysService&) = delete;
  MockPlatformKeysService& operator=(const MockPlatformKeysService&) = delete;
  ~MockPlatformKeysService() override;

  MOCK_METHOD(void,
              AddObserver,
              (PlatformKeysServiceObserver * observer),
              (override));

  MOCK_METHOD(void,
              RemoveObserver,
              (PlatformKeysServiceObserver * observer),
              (override));

  MOCK_METHOD(void,
              GenerateRSAKey,
              (TokenId token_id,
               unsigned int modulus_length_bits,
               GenerateKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              GenerateECKey,
              (TokenId token_id,
               const std::string& named_curve,
               GenerateKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Digest,
              (base::Optional<TokenId> token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               HashAlgorithm hash_algorithm,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Raw,
              (base::Optional<TokenId> token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SignECDSADigest,
              (base::Optional<TokenId> token_id,
               const std::string& data,
               const std::string& public_key_spki_der,
               HashAlgorithm hash_algorithm,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SelectClientCertificates,
              (const std::vector<std::string>& certificate_authorities,
               SelectCertificatesCallback callback),
              (override));

  MOCK_METHOD(void,
              GetCertificates,
              (TokenId token_id, GetCertificatesCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAllKeys,
              (TokenId token_id, GetAllKeysCallback callback),
              (override));

  MOCK_METHOD(void,
              ImportCertificate,
              (TokenId token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               ImportCertificateCallback callback),
              (override));

  MOCK_METHOD(void,
              RemoveCertificate,
              (TokenId token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               RemoveCertificateCallback callback),
              (override));

  MOCK_METHOD(void,
              RemoveKey,
              (TokenId token_id,
               const std::string& public_key_spki_der,
               RemoveKeyCallback callback),
              (override));

  MOCK_METHOD(void, GetTokens, (GetTokensCallback callback), (override));

  MOCK_METHOD(void,
              GetKeyLocations,
              (const std::string& public_key_spki_der,
               GetKeyLocationsCallback callback),
              (override));

  MOCK_METHOD(void,
              SetAttributeForKey,
              (TokenId token_id,
               const std::string& public_key_spki_der,
               KeyAttributeType attribute_type,
               const std::string& attribute_value,
               SetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAttributeForKey,
              (TokenId token_id,
               const std::string& public_key_spki_der,
               KeyAttributeType attribute_type,
               GetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              IsKeyOnToken,
              (TokenId token_id,
               const std::string& public_key_spki_der,
               IsKeyOnTokenCallback callback),
              (override));

  MOCK_METHOD(void,
              SetMapToSoftokenAttrsForTesting,
              (bool map_to_softoken_attrs_for_testing),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPlatformKeysService(
    content::BrowserContext* context);

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
