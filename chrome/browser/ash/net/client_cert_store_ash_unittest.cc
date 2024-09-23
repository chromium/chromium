// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_ash.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
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
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// "CN=B CA" - DER encoded DN of the issuer of client_1.pem
const unsigned char kAuthority1DN[] = {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b,
                                       0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                       0x04, 0x42, 0x20, 0x43, 0x41};

void SaveIdentitiesAndQuitCallback(net::ClientCertIdentityList* out_identities,
                                   base::OnceClosure quit_closure,
                                   net::ClientCertIdentityList in_identities) {
  *out_identities = std::move(in_identities);
  std::move(quit_closure).Run();
}

}  // namespace

class ClientCertStoreAshTest : public ::testing::Test {
 public:
  ClientCertStoreAshTest() {}

  void SetUp() override {
    ASSERT_TRUE(user1_.constructed_successfully());
    ASSERT_TRUE(user2_.constructed_successfully());
    ASSERT_TRUE(system_slot_.ConstructedSuccessfully());
  }

  scoped_refptr<net::X509Certificate> ImportCertToSlot(
      const std::string& cert_filename,
      const std::string& key_filename,
      PK11SlotInfo* slot) {
    return net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), cert_filename, key_filename, slot);
  }

 protected:
  crypto::ScopedTestNSSChromeOSUser user1_{"user1"};
  crypto::ScopedTestNSSChromeOSUser user2_{"user2"};
  crypto::ScopedTestSystemNSSKeySlot system_slot_{
      /*simulate_token_loader=*/true};
  content::BrowserTaskEnvironment task_environment_;
};

// Ensure that cert requests, that are started before the filter is initialized,
// will wait for the initialization and succeed afterwards.
TEST_F(ClientCertStoreAshTest, RequestWaitsForNSSInitAndSucceeds) {
  ClientCertStoreAsh store(nullptr /* no additional provider */,
                           /*use_system_slot=*/false, user1_.username_hash(),
                           ClientCertStoreAsh::PasswordDelegateFactory());

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
    // GetClientCerts should wait for the initialization of the filter to
    // finish.
    ASSERT_EQ(0u, selected_identities.size());
  }

  user1_.FinishInit();
  run_loop.Run();

  ASSERT_EQ(1u, selected_identities.size());
}

// Ensure that cert requests, that are started after the filter was initialized,
// will succeed.
TEST_F(ClientCertStoreAshTest, RequestsAfterNSSInitSucceed) {
  user1_.FinishInit();

  ClientCertStoreAsh store(nullptr /* no additional provider */,
                           /*use_system_slot=*/false, user1_.username_hash(),
                           ClientCertStoreAsh::PasswordDelegateFactory());

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

TEST_F(ClientCertStoreAshTest, Filter) {
  user1_.FinishInit();
  user2_.FinishInit();

  crypto::ScopedPK11Slot slot_public1 =
      crypto::GetPublicSlotForChromeOSUser(user1_.username_hash());
  crypto::ScopedPK11Slot slot_public2 =
      crypto::GetPublicSlotForChromeOSUser(user2_.username_hash());
  crypto::ScopedPK11Slot slot_private1 = crypto::GetPrivateSlotForChromeOSUser(
      user1_.username_hash(), base::DoNothing());
  crypto::ScopedPK11Slot slot_private2 = crypto::GetPrivateSlotForChromeOSUser(
      user2_.username_hash(), base::DoNothing());
  // crypto::GetPrivateSlotForChromeOSUser() may return nullptr if the slot is
  // not initialized yet, but this should not happen.
  ASSERT_TRUE(slot_private1);
  ASSERT_TRUE(slot_private2);

  // Import a certificate into each slot.
  scoped_refptr<net::X509Certificate> cert_public1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", slot_public1.get()));
  ASSERT_TRUE(cert_public1);
  scoped_refptr<net::X509Certificate> cert_private1(
      ImportCertToSlot("client_2.pem", "client_2.pk8", slot_private1.get()));
  ASSERT_TRUE(cert_private1);
  scoped_refptr<net::X509Certificate> cert_public2(
      ImportCertToSlot("client_3.pem", "client_3.pk8", slot_public2.get()));
  ASSERT_TRUE(cert_public2);
  scoped_refptr<net::X509Certificate> cert_private2(
      ImportCertToSlot("client_4.pem", "client_4.pk8", slot_private2.get()));
  ASSERT_TRUE(cert_private2);
  scoped_refptr<net::X509Certificate> cert_system(
      ImportCertToSlot("client_5.pem", "client_5.pk8", system_slot_.slot()));
  ASSERT_TRUE(cert_system);

  const struct FilterTest {
    bool use_system_slot;
    std::string username_hash;
    std::vector<raw_ptr<net::X509Certificate, VectorExperimental>> results;
  } kTests[] = {
      {false,
       user1_.username_hash(),
       {cert_public1.get(), cert_private1.get()}},
      {true,
       user1_.username_hash(),
       {cert_public1.get(), cert_private1.get(), cert_system.get()}},
      {false,
       user2_.username_hash(),
       {cert_public2.get(), cert_private2.get()}},
      {true,
       user2_.username_hash(),
       {cert_public2.get(), cert_private2.get(), cert_system.get()}},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.use_system_slot);
    SCOPED_TRACE(test.username_hash);

    ClientCertStoreAsh store(nullptr /* no additional provider */,
                             test.use_system_slot, test.username_hash,
                             ClientCertStoreAsh::PasswordDelegateFactory());

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
TEST_F(ClientCertStoreAshTest, CertRequestMatching) {
  user1_.FinishInit();

  ClientCertStoreAsh store(nullptr,  // no additional provider
                           /*use_system_slot=*/false, user1_.username_hash(),
                           ClientCertStoreAsh::PasswordDelegateFactory());

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

}  // namespace ash
