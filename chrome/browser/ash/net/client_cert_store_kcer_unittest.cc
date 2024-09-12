// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_kcer.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// "CN=B CA" - DER encoded DN of the issuer of client_1.pem
const unsigned char kAuthority1DN[] = {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b,
                                       0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                       0x04, 0x42, 0x20, 0x43, 0x41};

// "CN=C Root CA" - DER encoded DN of the issuer of client_1_ca.pem,
// client_2_ca.pem, and client_3_ca.pem.
const unsigned char kAuthorityRootDN[] = {
    0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 0x43, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41,
};

void SaveIdentitiesAndQuitCallback(net::ClientCertIdentityList* out_identities,
                                   base::OnceClosure quit_closure,
                                   net::ClientCertIdentityList in_identities) {
  *out_identities = std::move(in_identities);
  std::move(quit_closure).Run();
}

void SavePrivateKeyAndQuitCallback(scoped_refptr<net::SSLPrivateKey>* out_key,
                                   base::OnceClosure quit_closure,
                                   scoped_refptr<net::SSLPrivateKey> in_key) {
  *out_key = std::move(in_key);
  std::move(quit_closure).Run();
}

}  // namespace

class ClientCertStoreKcerTest : public ::testing::Test {
 public:
  ClientCertStoreKcerTest() {}

  void SetUp() override {
    ASSERT_TRUE(user1_.constructed_successfully());
    ASSERT_TRUE(user2_.constructed_successfully());
    ASSERT_TRUE(system_slot_.ConstructedSuccessfully());
    kcer_holder_.emplace(
        crypto::GetPublicSlotForChromeOSUser(user1_.username_hash()).get(),
        system_slot_.slot());
  }

  void TearDown() override { kcer_holder_.reset(); }

  scoped_refptr<net::X509Certificate> ImportCertToSlot(
      const std::string& cert_filename,
      const std::string& key_filename,
      PK11SlotInfo* slot) {
    scoped_refptr<net::X509Certificate> result =
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            cert_filename, key_filename, slot);
    // Kcer relies on notifications to invalidate caches when the client
    // certificate store changes.
    net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
    return result;
  }

 protected:
  crypto::ScopedTestNSSChromeOSUser user1_{"user1"};
  crypto::ScopedTestNSSChromeOSUser user2_{"user2"};
  crypto::ScopedTestSystemNSSKeySlot system_slot_{
      /*simulate_token_loader=*/true};
  std::optional<kcer::TestKcerHolder> kcer_holder_;
  content::BrowserTaskEnvironment task_environment_;
};

// Ensure that cert requests, that are started before NSS is initialized,
// will wait for the initialization and succeed afterwards.
TEST_F(ClientCertStoreKcerTest, RequestWaitsForNSSInitAndSucceeds) {
  ClientCertStoreKcer store(nullptr /* no additional provider */,
                            kcer_holder_->GetKcer());

  scoped_refptr<net::X509Certificate> cert_1(ImportCertToSlot(
      "client_1.pem", "client_1.pk8",
      crypto::GetPublicSlotForChromeOSUser(user1_.username_hash()).get()));
  ASSERT_TRUE(cert_1);

  // Request any client certificate, which is expected to match client_1.
  auto request_all = base::MakeRefCounted<net::SSLCertRequestInfo>();

  net::ClientCertIdentityList selected_identities;
  base::RunLoop run_loop;
  store.GetClientCerts(
      request_all,
      base::BindOnce(SaveIdentitiesAndQuitCallback, &selected_identities,
                     run_loop.QuitClosure()));

  {
    base::RunLoop run_loop_inner;
    run_loop_inner.RunUntilIdle();
    // GetClientCerts should wait for the initialization of NSS to finish.
    ASSERT_EQ(0u, selected_identities.size());
  }

  user1_.FinishInit();
  run_loop.Run();

  ASSERT_EQ(1u, selected_identities.size());
}

// Ensure that cert requests, that are started after NSS was initialized,
// will succeed.
TEST_F(ClientCertStoreKcerTest, RequestsAfterNSSInitSucceed) {
  user1_.FinishInit();

  ClientCertStoreKcer store(nullptr /* no additional provider */,
                            kcer_holder_->GetKcer());

  scoped_refptr<net::X509Certificate> cert_1(ImportCertToSlot(
      "client_1.pem", "client_1.pk8",
      crypto::GetPublicSlotForChromeOSUser(user1_.username_hash()).get()));
  ASSERT_TRUE(cert_1);

  auto request_all = base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::RunLoop run_loop;
  net::ClientCertIdentityList selected_identities;
  store.GetClientCerts(
      request_all,
      base::BindOnce(SaveIdentitiesAndQuitCallback, &selected_identities,
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, selected_identities.size());
}

TEST_F(ClientCertStoreKcerTest, Filter) {
  user1_.FinishInit();
  user2_.FinishInit();

  crypto::ScopedPK11Slot slot_private1 = crypto::GetPrivateSlotForChromeOSUser(
      user1_.username_hash(), base::DoNothing());
  crypto::ScopedPK11Slot slot_private2 = crypto::GetPrivateSlotForChromeOSUser(
      user2_.username_hash(), base::DoNothing());
  // crypto::GetPrivateSlotForChromeOSUser() may return nullptr if the slot is
  // not initialized yet, but this should not happen.
  ASSERT_TRUE(slot_private1);
  ASSERT_TRUE(slot_private2);

  kcer::TestKcerHolder user1_kcer_without_system_slot(slot_private1.get(),
                                                      nullptr);
  kcer::TestKcerHolder user1_kcer_with_system_slot(slot_private1.get(),
                                                   system_slot_.slot());
  kcer::TestKcerHolder user2_kcer_without_system_slot(slot_private2.get(),
                                                      nullptr);
  kcer::TestKcerHolder user2_kcer_with_system_slot(slot_private2.get(),
                                                   system_slot_.slot());

  // Import a certificate into each slot.
  scoped_refptr<net::X509Certificate> cert_private1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", slot_private1.get()));
  ASSERT_TRUE(cert_private1);
  scoped_refptr<net::X509Certificate> cert_private2(
      ImportCertToSlot("client_2.pem", "client_2.pk8", slot_private2.get()));
  ASSERT_TRUE(cert_private2);
  scoped_refptr<net::X509Certificate> cert_system(
      ImportCertToSlot("client_3.pem", "client_3.pk8", system_slot_.slot()));
  ASSERT_TRUE(cert_system);

  const struct FilterTest {
    std::string description;
    base::WeakPtr<kcer::Kcer> kcer;
    std::vector<raw_ptr<net::X509Certificate, VectorExperimental>> results;
  } kTests[] = {
      {"user1 without system slot",
       user1_kcer_without_system_slot.GetKcer(),
       {cert_private1.get()}},
      {"user1 with system slot",
       user1_kcer_with_system_slot.GetKcer(),
       {cert_private1.get(), cert_system.get()}},
      {"user2 without system slot",
       user2_kcer_without_system_slot.GetKcer(),
       {cert_private2.get()}},
      {"user2 with system slot",
       user2_kcer_with_system_slot.GetKcer(),
       {cert_private2.get(), cert_system.get()}},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.description);

    ClientCertStoreKcer store(nullptr /* no additional provider */, test.kcer);

    auto request_all = base::MakeRefCounted<net::SSLCertRequestInfo>();

    base::RunLoop run_loop;
    net::ClientCertIdentityList selected_identities;
    store.GetClientCerts(
        request_all,
        base::BindOnce(SaveIdentitiesAndQuitCallback, &selected_identities,
                       run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_EQ(test.results.size(), selected_identities.size());
    for (size_t i = 0; i < test.results.size(); i++) {
      bool found_cert = false;
      for (const auto& identity : selected_identities) {
        if (test.results[i]->EqualsExcludingChain(identity->certificate())) {
          found_cert = true;
          break;
        }
      }
      EXPECT_TRUE(found_cert) << "Could not find certificate " << i;
    }
  }
}

// Ensure that the delegation of the request matching to the base class is
// functional.
TEST_F(ClientCertStoreKcerTest, CertRequestMatching) {
  user1_.FinishInit();

  ClientCertStoreKcer store(nullptr,  // no additional provider
                            kcer_holder_->GetKcer());

  crypto::ScopedPK11Slot slot =
      crypto::GetPublicSlotForChromeOSUser(user1_.username_hash());
  scoped_refptr<net::X509Certificate> cert_1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", slot.get()));
  ASSERT_TRUE(cert_1);
  scoped_refptr<net::X509Certificate> cert_2(
      ImportCertToSlot("client_2.pem", "client_2.pk8", slot.get()));
  ASSERT_TRUE(cert_2.get());

  std::vector<std::string> authority_1 = {
      std::string(std::begin(kAuthority1DN), std::end(kAuthority1DN))};
  auto request = base::MakeRefCounted<net::SSLCertRequestInfo>();
  request->cert_authorities = authority_1;

  base::RunLoop run_loop;
  net::ClientCertIdentityList selected_identities;
  store.GetClientCerts(
      request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                              &selected_identities, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, selected_identities.size());
  EXPECT_TRUE(
      cert_1->EqualsExcludingChain(selected_identities[0]->certificate()));
}

// Tests that ClientCertStoreKcer attempts to build a certificate chain by
// querying NSS before return a certificate.
TEST_F(ClientCertStoreKcerTest, BuildsCertificateChain) {
  user1_.FinishInit();

  crypto::ScopedPK11Slot private_slot = crypto::GetPrivateSlotForChromeOSUser(
      user1_.username_hash(), base::DoNothing());
  // crypto::GetPrivateSlotForChromeOSUser() may return nullptr if the slot is
  // not initialized yet, but this should not happen.
  ASSERT_TRUE(private_slot);

  // Import client_1.pem into the slot of user1.
  scoped_refptr<net::X509Certificate> client_1(
      net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                          "client_1.pem", "client_1.pk8",
                                          private_slot.get()));
  ASSERT_TRUE(client_1.get());
  scoped_refptr<net::X509Certificate> client_1_ca(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1_ca.pem"));
  std::string pkcs8_key;
  ASSERT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("client_1.pk8"), &pkcs8_key));

  // Import client_1_ca.pem into a separate slot, that is visible to NSS in
  // general, but not to Kcer for user1.
  crypto::ScopedTestNSSDB public_slot;
  ASSERT_TRUE(client_1_ca.get());
  ASSERT_TRUE(net::ImportClientCertToSlot(client_1_ca, public_slot.slot()));

  ClientCertStoreKcer store(nullptr,  // no additional provider
                            kcer_holder_->GetKcer());

  // These test keys are RSA keys.
  std::vector<uint16_t> expected =
      net::SSLPrivateKey::DefaultAlgorithmPreferences(EVP_PKEY_RSA,
                                                      true /* supports PSS */);

  {
    // Request certificates matching B CA, |client_1|'s issuer.
    auto request = base::MakeRefCounted<net::SSLCertRequestInfo>();
    request->cert_authorities.emplace_back(
        reinterpret_cast<const char*>(kAuthority1DN), sizeof(kAuthority1DN));

    net::ClientCertIdentityList selected_identities;
    base::RunLoop loop;
    store.GetClientCerts(
        request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                                &selected_identities, loop.QuitClosure()));
    loop.Run();

    // The result be |client_1| with no intermediates.
    ASSERT_EQ(1u, selected_identities.size());
    scoped_refptr<net::X509Certificate> selected_cert =
        selected_identities[0]->certificate();
    EXPECT_TRUE(net::x509_util::CryptoBufferEqual(
        client_1->cert_buffer(), selected_cert->cert_buffer()));
    ASSERT_EQ(0u, selected_cert->intermediate_buffers().size());

    scoped_refptr<net::SSLPrivateKey> ssl_private_key;
    base::RunLoop key_loop;
    selected_identities[0]->AcquirePrivateKey(
        base::BindOnce(SavePrivateKeyAndQuitCallback, &ssl_private_key,
                       key_loop.QuitClosure()));
    key_loop.Run();

    ASSERT_TRUE(ssl_private_key);
    EXPECT_EQ(expected, ssl_private_key->GetAlgorithmPreferences());
    TestSSLPrivateKeyMatches(ssl_private_key.get(), pkcs8_key);
  }

  {
    // Request certificates matching C Root CA, |client_1_ca|'s issuer.
    auto request = base::MakeRefCounted<net::SSLCertRequestInfo>();
    request->cert_authorities.emplace_back(
        reinterpret_cast<const char*>(kAuthorityRootDN),
        sizeof(kAuthorityRootDN));

    net::ClientCertIdentityList selected_identities;
    base::RunLoop loop;
    store.GetClientCerts(
        request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                                &selected_identities, loop.QuitClosure()));
    loop.Run();

    // The result be |client_1| with |client_1_ca| as an intermediate.
    ASSERT_EQ(1u, selected_identities.size());
    scoped_refptr<net::X509Certificate> selected_cert =
        selected_identities[0]->certificate();
    EXPECT_TRUE(net::x509_util::CryptoBufferEqual(
        client_1->cert_buffer(), selected_cert->cert_buffer()));
    ASSERT_EQ(1u, selected_cert->intermediate_buffers().size());
    EXPECT_TRUE(net::x509_util::CryptoBufferEqual(
        client_1_ca->cert_buffer(),
        selected_cert->intermediate_buffers()[0].get()));

    scoped_refptr<net::SSLPrivateKey> ssl_private_key;
    base::RunLoop key_loop;
    selected_identities[0]->AcquirePrivateKey(
        base::BindOnce(SavePrivateKeyAndQuitCallback, &ssl_private_key,
                       key_loop.QuitClosure()));
    key_loop.Run();
    ASSERT_TRUE(ssl_private_key);
    EXPECT_EQ(expected, ssl_private_key->GetAlgorithmPreferences());
    TestSSLPrivateKeyMatches(ssl_private_key.get(), pkcs8_key);
  }
}

}  // namespace ash
