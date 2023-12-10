// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/net/cert_manager_impl.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace arc {

namespace {

constexpr char kPrivateKey[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDXt3N1RS4Ntj7o\n"
    "HucHcqO+nRuf/3dK1zDxhtnyrjBFBuhTTvNOL2Njm/LvB5EaaIc2UhavIPnnQEAt\n"
    "OmhvEsi8A3HB4EU+Pu6UJnUtmEPMC/9WTZnLYAA/gMKYZ8KPZQ1FqNi3pkeWcxTN\n"
    "twnTDEK4qtd6+1veqWTYuxU6IUNrE4GX1yhSV4fAq6PKqdTz7VLIZ/wasADMMZ/S\n"
    "o3/MDR7rHH9hVBPby6liEunXUjzT7L5t+ZN3vUejOlcqdfikuB73oPitZ1vfQ3Ux\n"
    "2+67hWMLXFrw+4JkBMxyHL2fLfGwczRMy82UVIgqJMYAOM5iTQeQcxVlqwAyEhFU\n"
    "XSXSF4PFAgMBAAECggEBAMF/spa39oagOq92wOACanVacmREARrmCuYsg6ZXr77L\n"
    "Ym0QPdmdUncQdYsKa5OXvenxGp3/Y4uXK7omUXWJEPztzgYOCa67PsEv+h5rHi2T\n"
    "eXhN5a3zsGVGN8gEExcTmyMoQTYDduWy1y9sh+iDb/o8bUvI23DQ3EA5GOJq4hHR\n"
    "6OODdEJiyDNXsE4cQ8lJ4/mGF+RS1cBkNHRWpZzWT3fZNSUQtMgYMrz72UR+usYY\n"
    "OzCQPLNPCYgUXUp3caR16qKxOMQgZKEoAAxxVooASnvmuQiMUEY81rJlCeFXa+h9\n"
    "/eN2eKUnwNac0kWf1SjFQEiFutdbyuVTDIg22UPnFsUCgYEA7mcLHBts1jmBp405\n"
    "UoMGmckfJecaX1VE2HcYUXrNOjOCD7LEQRo6yOYF7VfgM0LzzYOit7LN7ouZ6Hgt\n"
    "3zTuMVdvnyiD3BjgogjtppQ7LV/ALThSN+bApOAMtX6yF3QSVGPd7TpfrBz0/ROj\n"
    "8bcJ+3hpBIq44LS2Si3Mo3QWcecCgYEA56O5lJUxC6cDyZHM+k+AiLTCAA2A+L/P\n"
    "ToixW9xyssxsrVNtXXr+YYowGf/cgR3kAJ6NcYwFJfsJ27IFgbpX3pCFVAJF4bSb\n"
    "feE5e7qnYF4NSJspOEr/17VoFAxk7INO6yW7fNSirpY402L1ldQRa7lk5Qmvucj5\n"
    "TSBXB4fhv3MCgYBezEKyroUcuklAIvwEP23EgSENpVPrTLDPkqvs2nP5DLpPG7rG\n"
    "WHO/pxf8RNE2EQ15TzrI6STSEljlA8TZ2OZOYIJWO3oTbyEDzaESeCb/5+83DApF\n"
    "iFBaP21OTk7q3JDdVcjNqESa3/jbGZA7cZlakYrQ74iMcc96t7OD24mBSQKBgQCm\n"
    "14J/xsXAwtczhFTDpifKT4e8Sf2vLVjAFCzLIYlrx1ovrXuEbWZ0Evh6gZPtW/4x\n"
    "hAIU2umKZbrABwV4XyOTJz0hOVHkNBYbIPIqcFLGUnf25+tUpJCKahtA9Xxr7lgV\n"
    "fuQAEZfrcEAV4Z1KAalakfpeDhAIHP2T08tbnT+4iQKBgEI4YfylyTsMAc5RVVE7\n"
    "CIW3Gn3JSuWm3RRrDSeX3hJIZ8R7pF2rKYx7QYZi4p0fGny4Xw4bD1RWJwt9mRuE\n"
    "WYG/EA9Q2PLsfyd8SmUTbnXU6JYUalu3apECNGg2EoXHk9OODhqYphlzSNEHP0LT\n"
    "fuHaboQVbHNmCQR8DRZgzGH/\n"
    "-----END PRIVATE KEY-----";

constexpr char kWrongPrivateKey[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDZmo7mtBcxsvMp\n"
    "T5a/p3L/1byrgyAd4zIDR3ivjf6anNlMVNTGSxdtrvCCRxMZPIrEOlq8kIzFqvXn\n"
    "kZNzZms/ofrQ0c0K7aXhaH+zH3x7eMMZLCyl/gBjhThZac6BtlP6+muHCTL3EKLO\n"
    "qpPFpkYjnYI3g2qj5dgDJcfhqF0RL5CXHSdHWc2OSK2C5krqT4nPu8tKhHiAOLRW\n"
    "BBJQ3jpl7rlH1SqKtba3ppu4AioOTtlHfCcR0XZ3ym55QGufIci2yr9pvCwVRwXO\n"
    "VwisNEyRmfEOXuSUyx6ZZy/vDeqtwLSuSL19gMsTk+HK1dpijnwzlowOwo+7ixZ4\n"
    "G9g9jjivAgMBAAECggEAdcS1ZFzBVM+B1LDTaIRqs9Vsl/KOlj5Y2fd7dJ/H1Mvg\n"
    "uvQKeAs58c3FMuzehEEE5TCj3PvqhCyDi8F46PLcRoMW6J8zdp+psDXLLxlyWKzC\n"
    "AkSrIWc3tKTsG1AtSHxyNRoEyf+LirWBN5KQCV91BF+BkyPXuj5xyzpOVG23eM2i\n"
    "QDdvvhqetGT0coM0ZrxyTO8hOqtwuBIV+b+PUDXHfiGbIhkt+9ZJL8T0WXOoHNII\n"
    "lrf9t06FNFiFnSwEGhBIi/BiZ83wgJV67IH4yUO3VHYqRSfQCNWNfzqeOUc3X7VU\n"
    "eiMyQw//shq9Wmx9R70crLDP6B/8voKMb/CCUK8DmQKBgQD85rtWx1IzdkDaDhNX\n"
    "samV7JKppeUijTc/jv6K18EAyWa7iICWwcUw18eRWSYq4JbWF3936OX4RLeZmUoI\n"
    "4jqRAH9Xlnfg7ceygKunKXLlm+TFpyBBtlrGsYhUb1kUskYdPgvY/5NpI9HsE1hy\n"
    "xPZBqoQ3u3wGJxTdQPsh35moDQKBgQDcRRwOkxTa0ijkDapyGXDJrR6fF9Tm2HJJ\n"
    "wTNLnLtxjFseHs1ZKefY5dbTeISNRlCye3xhrVTsbV1YlVItQVBfi1Qv1lMUhU3j\n"
    "e06Ra1aKsVUDBMgv8jJBZU3Jx8ytkltq42HSbKk1BzCheNtfV7YXi8vcJV9fwCIa\n"
    "ZI45BKDYqwKBgDYHUQSEBqKp48bx9N3qPbGi3d5Sa7ZK9v+kG+srlrcFT+ZGjjom\n"
    "4WrC3obFxeqpGnBYisniPqcgfxzYa8GkGyD5OztKEQhDpEMVTBalOz+kY2Z6guCn\n"
    "BZOnP9nSA/Tw9RuwMrXEPAjdNy65H089luKGfEKv0ho6ZTGzfTNKYrhNAoGARlDF\n"
    "gR2Qxb3bEdoO9DeM2sSqBs17yGmGKmdDcbrJ15ifqcDZesI24fWVG5LYdaThs+hZ\n"
    "r3C+sG7FIrcgMZQtDSMUL+UyRlW7pIfDcAac7M9pPPp00WF2i4vERkrC2xHinv+R\n"
    "RbQsW+I8sv86wHfmiCO3Y0KG7LEP8e7xu9/vXNsCgYEA7/EI7EklUB4jIpEJ7oiq\n"
    "IWIaEaUp6kcopTAKinDpTfVkKZRxtCu70SQL+55ovINA8UvrArGXiCwRfMIW/+Vv\n"
    "vO6JLBdJU9XKpN+/OP6kdAivlQLuZNIlrMG8eP80KoV4ckQh0Gp2yNoLC+Vuyfjc\n"
    "iow7aHtm4m4vjWVETAHFviw=\n"
    "-----END PRIVATE KEY-----";

constexpr char kUserCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDazCCAlOgAwIBAgIUVLGPFfkgeQbMUT0k/FX3sL101dAwDQYJKoZIhvcNAQEL\n"
    "BQAwRTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM\n"
    "GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMjAyMTYxMzIwMjdaFw0yMjAz\n"
    "MTgxMzIwMjdaMEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw\n"
    "HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQDXt3N1RS4Ntj7oHucHcqO+nRuf/3dK1zDxhtnyrjBF\n"
    "BuhTTvNOL2Njm/LvB5EaaIc2UhavIPnnQEAtOmhvEsi8A3HB4EU+Pu6UJnUtmEPM\n"
    "C/9WTZnLYAA/gMKYZ8KPZQ1FqNi3pkeWcxTNtwnTDEK4qtd6+1veqWTYuxU6IUNr\n"
    "E4GX1yhSV4fAq6PKqdTz7VLIZ/wasADMMZ/So3/MDR7rHH9hVBPby6liEunXUjzT\n"
    "7L5t+ZN3vUejOlcqdfikuB73oPitZ1vfQ3Ux2+67hWMLXFrw+4JkBMxyHL2fLfGw\n"
    "czRMy82UVIgqJMYAOM5iTQeQcxVlqwAyEhFUXSXSF4PFAgMBAAGjUzBRMB0GA1Ud\n"
    "DgQWBBS1UScRUjNDA88mUXUuzwOJ35aCnDAfBgNVHSMEGDAWgBS1UScRUjNDA88m\n"
    "UXUuzwOJ35aCnDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCf\n"
    "gcfXjLfaOots4AFtd4sQglWA0RaIfaZl1QTAp6QvJQY7jFyyeNuYV3DfT2DCzJOY\n"
    "NwRluNCfUZG/YbpTtMUDODpDjASF0z9kQc1bVg3NdcscI+LFtMivuvG7v3Bp7G3I\n"
    "bhnbhDmWRU9Wss4P3F7x2EULX6NwUzcmyHtEQ+A9xjm6BpshCx3qZNEOYFqV7U82\n"
    "ioxoifZ5JDF7fIkF22rsI+Ufo3mMvjYz5vSRxc0OVmbpfgmnhB5ValEAopwP5FN3\n"
    "Jn8C5u+DExI0s93xzNMmJL0ONVeQORY8YkV+7E8wuD+VLSuoo4S/VZ108DRl+kxx\n"
    "e8AkIK+5yjLpD/0P4TN0\n"
    "-----END CERTIFICATE-----";

class ImportDoneWaiter
    : public base::test::TestFuture<std::optional<std::string>,
                                    std::optional<int>> {
 public:
  CertManager::ImportPrivateKeyAndCertCallback GetCallback() {
    return TestFuture::GetCallback<const std::optional<std::string>&,
                                   const std::optional<int>&>();
  }

  std::optional<std::string> imported_cert_id() { return Get<0>(); }
  std::optional<int> imported_slot_id() { return Get<1>(); }
};

}  // namespace

class CertManagerImplTest : public testing::Test {
 public:
  CertManagerImplTest() = default;

  CertManagerImplTest(const CertManagerImplTest&) = delete;
  CertManagerImplTest& operator=(const CertManagerImplTest&) = delete;

  ~CertManagerImplTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    cert_manager_ = std::make_unique<CertManagerImpl>(profile());
    nss_db_ = std::make_unique<crypto::ScopedTestNSSDB>();
    cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_db_->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_db_->slot())));
  }

  void TearDown() override {
    // Ensure that nothing is running before tearing down.
    base::RunLoop().RunUntilIdle();

    profile_.reset();
    cert_manager_.reset();
    cert_db_.reset();
    nss_db_.reset();
  }

  Profile* profile() { return profile_.get(); }
  CertManagerImpl* cert_manager() { return cert_manager_.get(); }
  net::NSSCertDatabase* cert_db() { return cert_db_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CertManagerImpl> cert_manager_;

  std::unique_ptr<crypto::ScopedTestNSSDB> nss_db_;
  std::unique_ptr<net::NSSCertDatabase> cert_db_;

  content::BrowserTaskEnvironment task_environment_;
};

// Imports with a valid certificate and key succeed.
TEST_F(CertManagerImplTest, ImportKeyAndCertTest) {
  ImportDoneWaiter import_future;
  cert_manager()->ImportPrivateKeyAndCertWithDB(
      kPrivateKey, kUserCert, import_future.GetCallback(), cert_db());
  EXPECT_TRUE(import_future.Wait());
  EXPECT_TRUE(import_future.imported_cert_id().has_value());
  EXPECT_TRUE(import_future.imported_slot_id().has_value());

  // Assert that the imported key and certificate have the same ID.
  bssl::PEMTokenizer tokenizer(kUserCert, {kCertificatePEMHeader});
  EXPECT_TRUE(tokenizer.GetNext());
  std::vector<uint8_t> cert_der(tokenizer.data().begin(),
                                tokenizer.data().end());
  net::ScopedCERTCertificate cert(
      net::x509_util::CreateCERTCertificateFromBytes(cert_der));

  // Get the certificate ID.
  crypto::ScopedSECItem cert_sec_item(
      PK11_GetLowLevelKeyIDForCert(nullptr, cert.get(), nullptr));
  EXPECT_TRUE(cert_sec_item);
  std::string cert_id =
      base::HexEncode(cert_sec_item->data, cert_sec_item->len);

  // Get the key ID.
  crypto::ScopedPK11Slot private_slot = cert_db()->GetPrivateSlot();
  EXPECT_TRUE(private_slot);
  crypto::ScopedSECKEYPrivateKey key(
      PK11_FindPrivateKeyFromCert(private_slot.get(), cert.get(), nullptr));
  EXPECT_TRUE(key);
  crypto::ScopedSECItem key_sec_item(
      PK11_GetLowLevelKeyIDForPrivateKey(key.get()));
  EXPECT_TRUE(key_sec_item);
  std::string key_id = base::HexEncode(key_sec_item->data, key_sec_item->len);

  EXPECT_EQ(key_id, cert_id);
  EXPECT_EQ(import_future.imported_cert_id().value(), cert_id);
  EXPECT_EQ(import_future.imported_slot_id().value(),
            static_cast<int>(PK11_GetSlotID(private_slot.get())));
}

// Importing a certificate with the wrong key fail.
TEST_F(CertManagerImplTest, ImportCertWithWrongKeyTest) {
  ImportDoneWaiter import_future;
  cert_manager()->ImportPrivateKeyAndCertWithDB(
      kWrongPrivateKey, kUserCert, import_future.GetCallback(), cert_db());
  EXPECT_TRUE(import_future.Wait());
  EXPECT_FALSE(import_future.imported_cert_id().has_value());
  EXPECT_FALSE(import_future.imported_slot_id().has_value());
}

// Imports with invalid certificate database fail.
TEST_F(CertManagerImplTest, ImportKeyAndCertWithInvalidDBTest) {
  ImportDoneWaiter import_future;
  cert_manager()->ImportPrivateKeyAndCertWithDB(kPrivateKey, kUserCert,
                                                import_future.GetCallback(),
                                                /*database=*/nullptr);
  EXPECT_TRUE(import_future.Wait());
  EXPECT_FALSE(import_future.imported_cert_id().has_value());
  EXPECT_FALSE(import_future.imported_slot_id().has_value());
}

// Imports with invalid certificates or keys fail.
TEST_F(CertManagerImplTest, ImportInvalidDataTest) {
  ImportDoneWaiter import_future;
  cert_manager()->ImportPrivateKeyAndCertWithDB(
      /*key_pem=*/"", /*cert_pem=*/"", import_future.GetCallback(), cert_db());
  EXPECT_TRUE(import_future.Wait());
  EXPECT_FALSE(import_future.imported_cert_id().has_value());
  EXPECT_FALSE(import_future.imported_slot_id().has_value());
}

}  // namespace arc
