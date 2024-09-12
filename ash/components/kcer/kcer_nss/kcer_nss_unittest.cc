// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "ash/components/kcer/chaps/mock_high_level_chaps_client.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_histograms.h"
#include "ash/components/kcer/kcer_impl.h"
#include "ash/components/kcer/kcer_nss/kcer_token_impl_nss.h"
#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "ash/components/kcer/kcer_token_utils.h"
#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ref.h"
#include "base/task/bind_post_task.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/secure_hash.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

using testing::DoAll;
using testing::UnorderedElementsAre;
using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using SlotId = kcer::SessionChapsClient::SlotId;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using kcer::MakeSpan;
using kcer::internal::KcerPkcs12ImportEvent;
using pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss;
using testing::_;

namespace kcer {

// Test-only overloads for better errors from EXPECT_EQ, etc.
std::ostream& operator<<(std::ostream& stream, Error val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, Token val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, PublicKey val) {
  stream << "{\n";
  stream << "  token: " << val.GetToken() << "\n";
  stream << "  pkcs11_id: " << base::Base64Encode(val.GetPkcs11Id().value())
         << "\n";
  stream << "  spki: " << base::Base64Encode(val.GetSpki().value()) << "\n";
  stream << "}\n";
  return stream;
}

namespace {

enum class TestKeyType {
  kRsa,
  kEcc,
  kImportedRsa,
  kImportedEcc,
};

constexpr char kPublicKeyBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArURIGgAq8joyzjFdUpzmOeDa5VgTC8"
    "n77sMCQsm01mwk+6NwHhCSyCfXoB9EuMcKynj9SZbCgArnsHcZiqBsKpU/VnBO/"
    "vp5MSY5qFMYxEpjPYSQcASUkOlkVYieQN6NK4FUynPJBIh3Rs6LUHlGU+"
    "w3GifCl3Be4Q0om61Eo+jxQJBlRFTyqETh0AeHI2lEK9hsePsn8AMJn2tv7GoaiS+"
    "RoZsMAcDg8uhtmlQB/"
    "eoy7MtXwSchI0e2Q8QdUneNp529Ee+pUQ5Uki1L2pE4Pnyj+j2i2x4wGFGdJgiBMSvtpvdPdF+"
    "NMfjdbVaDzTF3rcL3lNCxRb4xk3TMFXV7dQIDAQAB";

// Password for client.p12 and 2_client_certs_1_key.p12 .
constexpr char kPkcs12RsaFilePassword[] = "12345";
// Password for client_with_ec_key.p12 .
constexpr char kPkcs12EcFilePassword[] = "123456";

constexpr int kDefaultAttempts = 5;

std::string TestKeyTypeToStr(TestKeyType key_type) {
  switch (key_type) {
    case TestKeyType::kRsa:
      return "kRsa";
    case TestKeyType::kEcc:
      return "kEcc";
    case TestKeyType::kImportedRsa:
      return "kImportedRsa";
    case TestKeyType::kImportedEcc:
      return "kImportedEcc";
  }
}

std::vector<uint8_t> StrToBytes(const std::string& val) {
  return std::vector<uint8_t>(val.begin(), val.end());
}

scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
  return content::GetIOThreadTaskRunner({});
}

bool SpanEqual(base::span<const uint8_t> s1, base::span<const uint8_t> s2) {
  return base::ranges::equal(s1, s2);
}
bool SpanEqual(base::span<const char> s1, base::span<const uint8_t> s2) {
  return base::ranges::equal(base::as_bytes(s1), s2);
}

std::string ToString(const std::vector<SigningScheme>& vec) {
  std::stringstream res;
  res << "[";
  for (const SigningScheme& s : vec) {
    res << static_cast<int>(s) << ", ";
  }
  res << "]";
  return res.str();
}

std::unique_ptr<kcer::Kcer> CreateKcer(
    scoped_refptr<base::TaskRunner> token_task_runner,
    base::WeakPtr<kcer::internal::KcerToken> user_token,
    base::WeakPtr<kcer::internal::KcerToken> device_token) {
  auto kcer = std::make_unique<kcer::internal::KcerImpl>();
  kcer->Initialize(std::move(token_task_runner), std::move(user_token),
                   std::move(device_token));
  return kcer;
}

bool KeyInfoEquals(const KeyInfo& expected, const KeyInfo& actual) {
  if (expected.is_hardware_backed != actual.is_hardware_backed) {
    LOG(ERROR) << "ERROR: is_hardware_backed: expected: "
               << expected.is_hardware_backed
               << ", actual: " << actual.is_hardware_backed;
    return false;
  }
  if (expected.key_type != actual.key_type) {
    LOG(ERROR) << "ERROR: key_type: expected: " << int(expected.key_type)
               << ", actual: " << int(actual.key_type);
    return false;
  }
  if (expected.supported_signing_schemes != actual.supported_signing_schemes) {
    LOG(ERROR) << "ERROR: supported_signing_schemes: expected: "
               << ToString(expected.supported_signing_schemes)
               << ", actual: " << ToString(actual.supported_signing_schemes);
    return false;
  }
  if (expected.nickname != actual.nickname) {
    LOG(ERROR) << "ERROR: nickname: expected: "
               << expected.nickname.value_or("<empty>")
               << ", actual: " << actual.nickname.value_or("<empty>");
    return false;
  }
  return true;
}

// A helper class for receiving notifications from Kcer.
class NotificationsObserver {
 public:
  explicit NotificationsObserver(base::test::TaskEnvironment& task_environment)
      : task_environment_(task_environment) {}

  base::RepeatingClosure GetCallback() {
    return base::BindRepeating(&NotificationsObserver::OnCertDbChanged,
                               weak_factory_.GetWeakPtr());
  }

  void OnCertDbChanged() {
    notifications_counter_++;
    if (run_loop_ &&
        notifications_counter_ >= expected_notifications_.value()) {
      run_loop_->Quit();
    }
  }

  // Waits until the required number of notifications is received. Tries to
  // check that no extra notifications is sent.
  bool WaitUntil(size_t notifications) {
    if (notifications_counter_ < notifications) {
      expected_notifications_ = notifications;
      run_loop_.emplace();
      run_loop_->Run();
      run_loop_.reset();
      expected_notifications_.reset();
    }

    // An additional RunUntilIdle to try catching extra unwanted notifications.
    task_environment_->RunUntilIdle();

    if (notifications_counter_ != notifications) {
      LOG(ERROR) << "Actual notifications: " << notifications_counter_;
      return false;
    }
    return true;
  }

  size_t Notifications() const { return notifications_counter_; }

 private:
  const raw_ref<base::test::TaskEnvironment> task_environment_;
  size_t notifications_counter_ = 0;
  std::optional<base::RunLoop> run_loop_;
  std::optional<size_t> expected_notifications_;
  base::WeakPtrFactory<NotificationsObserver> weak_factory_{this};
};

// Test fixture for KcerNss tests. Provides the least amount of pre-configured
// setup to give more control to the tests themself.
class KcerNssTest : public testing::Test {
 public:
  KcerNssTest() : observer_(task_environment_) {}

 protected:
  void InitializeKcer(std::vector<Token> tokens) {
    base::WeakPtr<internal::KcerToken> user_token_ptr;
    base::WeakPtr<internal::KcerToken> device_token_ptr;

    for (Token token_type : tokens) {
      if (token_type == Token::kUser) {
        CHECK(!user_token_ptr.MaybeValid());
        user_token_ = std::make_unique<TokenHolder>(token_type, &chaps_client_,
                                                    /*initialize=*/true);
        user_token_ptr = user_token_->GetWeakPtr();
      } else if (token_type == Token::kDevice) {
        CHECK(!device_token_ptr.MaybeValid());
        device_token_ = std::make_unique<TokenHolder>(
            token_type, &chaps_client_, /*initialize=*/true);
        device_token_ptr = device_token_->GetWeakPtr();
      }
    }

    kcer_ = CreateKcer(IOTaskRunner(), user_token_ptr, device_token_ptr);
    observers_subscription_ = kcer_->AddObserver(observer_.GetCallback());
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  NotificationsObserver observer_;
  base::CallbackListSubscription observers_subscription_;
  MockHighLevelChapsClient chaps_client_;
  std::unique_ptr<TokenHolder> user_token_;
  std::unique_ptr<TokenHolder> device_token_;
  std::unique_ptr<Kcer> kcer_;
  base::HistogramTester histogram_tester_;
};

// Test that if a method is called with a token that is not (and won't be)
// available, then an error is returned.
TEST_F(KcerNssTest, UseUnavailableTokenThenGetError) {
  InitializeKcer(/*tokens=*/{});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_waiter.GetCallback());

  ASSERT_FALSE(generate_waiter.Get().has_value());
  EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that all methods can be queued while a Kcer instance and its token are
// initializing and that the entire task queue can be processed when
// initialization completes (in this case - completes with a failure).
TEST_F(KcerNssTest, QueueTasksThenFailInitializationThenGetErrors) {
  // Do not initialize yet to simulate slow initialization.
  TokenHolder user_token(Token::kUser, &chaps_client_, /*initialize=*/false);

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  // Internal values don't matter, they won't be accessed during this test.
  scoped_refptr<Cert> fake_cert = base::MakeRefCounted<Cert>(
      Token::kUser, Pkcs11Id(), /*nickname=*/std::string(),
      /*x509_cert=*/nullptr);

  // Create a Kcer instance without any tokens. It will queue all the incoming
  // requests itself.
  auto kcer = std::make_unique<kcer::internal::KcerImpl>();
  auto subscription = kcer->AddObserver(observer_.GetCallback());

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_rsa_waiter;
  kcer->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_ec_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true,
                      generate_ec_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer({1, 2, 3}),
                  import_key_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>>
      import_cert_from_bytes_waiter;
  kcer->ImportCertFromBytes(Token::kUser, CertDer({1, 2, 3}),
                            import_cert_from_bytes_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> import_x509_cert_waiter;
  kcer->ImportX509Cert(Token::kUser,
                       /*cert=*/cert_builder->GetX509Certificate(),
                       import_x509_cert_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> import_pkcs12_cert_waiter;
  kcer->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(), "password",
                         /*hardware_backed=*/true, /*mark_as_migrated=*/true,
                         import_pkcs12_cert_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>>
      remove_key_and_certs_waiter;
  kcer->RemoveKeyAndCerts(PrivateKeyHandle(PublicKeySpki()),
                          remove_key_and_certs_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
  kcer->RemoveCert(fake_cert, remove_cert_waiter.GetCallback());
  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_keys_waiter;
  kcer->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      list_certs_waiter;
  kcer->ListCerts({Token::kUser}, list_certs_waiter.GetCallback());
  base::test::TestFuture<base::expected<bool, Error>> does_key_exist_waiter;
  kcer->DoesPrivateKeyExist(PrivateKeyHandle(PublicKeySpki()),
                            does_key_exist_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer->Sign(PrivateKeyHandle(PublicKeySpki()), SigningScheme::kRsaPkcs1Sha512,
             DataToSign({1, 2, 3}), sign_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_digest_waiter;
  kcer->SignRsaPkcs1Raw(PrivateKeyHandle(PublicKeySpki()),
                        DigestWithPrefix({1, 2, 3}),
                        sign_digest_waiter.GetCallback());
  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<KeyInfo, Error>> get_key_info_waiter;
  kcer->GetKeyInfo(PrivateKeyHandle(PublicKeySpki()),
                   get_key_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
  kcer->SetKeyNickname(PrivateKeyHandle(PublicKeySpki()), "new_nickname",
                       set_nickname_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
  kcer->SetKeyPermissions(PrivateKeyHandle(PublicKeySpki()),
                          chaps::KeyPermissions(),
                          set_permissions_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_cert_prov_waiter;
  kcer->SetCertProvisioningProfileId(PrivateKeyHandle(PublicKeySpki()),
                                     "cert_prov_id",
                                     set_cert_prov_waiter.GetCallback());
  // Close the list with one more GenerateRsaKey, so all methods are tested
  // with other methods before and after them.
  base::test::TestFuture<base::expected<PublicKey, Error>>
      generate_rsa_waiter_2;
  kcer->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter_2.GetCallback());
  // TODO(244408716): Add more methods when they are implemented.

  // Check some waiters that the requests are queued.
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(list_keys_waiter.IsReady());
  EXPECT_FALSE(set_permissions_waiter.IsReady());
  EXPECT_FALSE(import_pkcs12_cert_waiter.IsReady());

  // Initialize Kcer with a token. This will empty the queue in `kcer` and move
  // all the requests into the token.
  kcer->Initialize(IOTaskRunner(), user_token.GetWeakPtr(),
                   /*device_token=*/nullptr);
  task_environment_.RunUntilIdle();

  // Check some waiters that the requests are still queued.
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(list_keys_waiter.IsReady());
  EXPECT_FALSE(set_permissions_waiter.IsReady());
  EXPECT_FALSE(import_pkcs12_cert_waiter.IsReady());

  // This should process and fail all the requests.
  user_token.FailTokenInitialization();

  ASSERT_FALSE(generate_rsa_waiter.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_ec_waiter.Get().has_value());
  EXPECT_EQ(generate_ec_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_key_waiter.Get().has_value());
  EXPECT_EQ(import_key_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_cert_from_bytes_waiter.Get().has_value());
  EXPECT_EQ(import_cert_from_bytes_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_x509_cert_waiter.Get().has_value());
  EXPECT_EQ(import_x509_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_pkcs12_cert_waiter.Get().has_value());
  EXPECT_EQ(import_pkcs12_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(remove_key_and_certs_waiter.Get().has_value());
  EXPECT_EQ(remove_key_and_certs_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(remove_cert_waiter.Get().has_value());
  EXPECT_EQ(remove_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_keys_waiter.Get<1>().empty());
  EXPECT_EQ(list_keys_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_certs_waiter.Get<1>().empty());
  EXPECT_EQ(list_certs_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(does_key_exist_waiter.Get().has_value());
  EXPECT_EQ(does_key_exist_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_digest_waiter.Get().has_value());
  EXPECT_EQ(sign_digest_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_token_info_waiter.Get().has_value());
  EXPECT_EQ(get_token_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_key_info_waiter.Get().has_value());
  EXPECT_EQ(get_key_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_nickname_waiter.Get().has_value());
  EXPECT_EQ(set_nickname_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_permissions_waiter.Get().has_value());
  EXPECT_EQ(set_permissions_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_cert_prov_waiter.Get().has_value());
  EXPECT_EQ(set_cert_prov_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_rsa_waiter_2.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter_2.Get().error(),
            Error::kTokenInitializationFailed);
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer forwards notifications from external sources. (Notifications
// created by Kcer are tested together with the methods that create them.)
TEST_F(KcerNssTest, ObserveExternalNotification) {
  TokenHolder user_token(Token::kUser, &chaps_client_, /*initialize=*/true);

  std::unique_ptr<Kcer> kcer =
      CreateKcer(IOTaskRunner(), user_token.GetWeakPtr(),
                 /*device_token=*/nullptr);

  NotificationsObserver observer_1(task_environment_);
  NotificationsObserver observer_2(task_environment_);

  // Add the first observer.
  auto subscription_1 = kcer->AddObserver(observer_1.GetCallback());

  EXPECT_EQ(observer_1.Notifications(), 0u);

  // Check that it receives a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/1));

  // Add one more observer.
  auto subscription_2 = kcer->AddObserver(observer_2.GetCallback());

  // Check that both of them receive a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/2));
  EXPECT_TRUE(observer_2.WaitUntil(/*notifications=*/1));

  // Destroy the first subscription, the first observer should stop receiving
  // notifications now.
  subscription_1 = base::CallbackListSubscription();

  // Check that only the second observer receives a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_2.WaitUntil(/*notifications=*/2));
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/2));
}

TEST_F(KcerNssTest, ListKeys) {
  InitializeKcer({Token::kUser, Token::kDevice});

  std::vector<PublicKey> all_expected_keys;
  std::vector<PublicKey> user_expected_keys;
  std::vector<PublicKey> device_expected_keys;

  // Initially there should be no keys.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());

    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                          /*hardware_backed=*/true,
                          generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // The new key should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key on a different token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateRsaKey(Token::kDevice, RsaModulusLength::k2048,
                          /*hardware_backed=*/true,
                          generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key of a different type on user token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Generate a key of a different type on device token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys of both types from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Keys of both types only from the user token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(user_expected_keys));
  }

  // Keys of both types only from the device token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kDevice}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(device_expected_keys));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::Sign() works correctly for RSA keys with different signing
// schemes.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignRsa) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kRsaPkcs1Sha1 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  // Test kRsaPkcs1Sha256 signature. Save signature to compare with it later.
  Signature rsa256_signature;
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    rsa256_signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, rsa256_signature));
  }

  // Test kRsaPssRsaeSha256 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPssRsaeSha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  // Test `Kcer::SignRsaPkcs1Raw()` (kRsaPkcs1Sha256, but for pre-hashed
  // values).
  {
    // A caller would need to hash the data themself before calling
    // `SignRsaPkcs1Digest`, do that here.
    auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hasher->Update(data_to_sign->data(), data_to_sign->size());
    std::vector<uint8_t> hash(hasher->GetHashLength());
    hasher->Finish(hash.data(), hash.size());
    DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

    // Generate the signature.
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->SignRsaPkcs1Raw(PrivateKeyHandle(public_key),
                           std::move(digest_with_prefix),
                           sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    // Verify the signature.
    EXPECT_TRUE(VerifySignature(SigningScheme::kRsaPkcs1Sha256,
                                public_key.GetSpki(), data_to_sign, signature));
    // Manual hashing + `SignRsaPkcs1Digest` should produce the same signature
    // as just `Sign`.
    EXPECT_EQ(signature, rsa256_signature);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::Sign() works correctly for ECC keys.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignEcc) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true,
                       generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kEcdsaSecp256r1Sha256 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kEcdsaSecp256r1Sha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that a certificate can not be imported, if there's no key for it on
// the token.
TEST_F(KcerNssTest, ImportCertWithoutKeyThenFail) {
  InitializeKcer({Token::kUser});

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  CertDer cert(StrToBytes(cert_builder->GetDER()));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportCertFromBytes(Token::kUser, std::move(cert),
                             import_waiter.GetCallback());
  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);

  // Double check that ListCerts doesn't find the cert.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<0>().empty());  // Cert list is empty.
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that a certificate can not be imported, if there's no key for it on
// the token.
TEST_F(KcerNssTest, ImportCertWithKeyOnDifferentTokenThenFail) {
  InitializeKcer({Token::kUser, Token::kDevice});

  // Generate new key on the user token.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      MakeCertBuilder(issuer.get(), public_key.GetSpki().value());

  // Import a cert for the key into the device token.
  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportX509Cert(Token::kDevice, cert_builder->GetX509Certificate(),
                        import_waiter.GetCallback());
  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);

  // Double check that ListCerts doesn't find the cert.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<0>().empty());  // Cert list is empty.
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::GetTokenInfo() method returns meaningful values.
TEST_F(KcerNssTest, GetTokenInfo) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer_->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  ASSERT_TRUE(get_token_info_waiter.Get().has_value());
  const TokenInfo& token_info = get_token_info_waiter.Get().value();

  // These values don't have to be exactly like this, they are what a software
  // NSS slot returns in tests. Still useful to test that they are not
  // completely off.
  EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
  EXPECT_THAT(token_info.token_name,
              testing::StartsWith("NSS Application Slot"));
  EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test RSA specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForRsaKey) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                    key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kRsa);
  EXPECT_THAT(
      key_info.supported_signing_schemes,
      UnorderedElementsAre(
          SigningScheme::kRsaPkcs1Sha1, SigningScheme::kRsaPkcs1Sha256,
          SigningScheme::kRsaPkcs1Sha384, SigningScheme::kRsaPkcs1Sha512,
          SigningScheme::kRsaPssRsaeSha256, SigningScheme::kRsaPssRsaeSha384,
          SigningScheme::kRsaPssRsaeSha512));
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test ECC specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForEccKey) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                    key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kEcc);
  EXPECT_THAT(key_info.supported_signing_schemes,
              UnorderedElementsAre(SigningScheme::kEcdsaSecp256r1Sha256,
                                   SigningScheme::kEcdsaSecp384r1Sha384,
                                   SigningScheme::kEcdsaSecp521r1Sha512));
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test generic fields from GetKeyInfo's result and that they get updated after
// related Set* methods. Test getters for custom attributes.
TEST_F(KcerNssTest, GetKeyInfoGenericAndCustomAttributes) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  KeyInfo expected_key_info;
  // Hardware- vs software-backed indicators on real devices are provided by
  // Chaps and are wrong in unit tests.
  expected_key_info.is_hardware_backed = true;
  // NSS sets an empty nickname by default, this doesn't have to be like this
  // in general.
  expected_key_info.nickname = "";
  // Custom attributes are stored differently in tests and have empty values by
  // default.
  chaps::KeyPermissions expected_key_permissions = chaps::KeyPermissions();
  std::string expected_cert_provisioning_profile_id = "";

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    const KeyInfo& key_info = key_info_waiter.Get().value();

    // Copy some fields, their values are covered by dedicated tests, this
    // test only checks that they don't change when they shouldn't.
    expected_key_info.key_type = key_info.key_type;
    expected_key_info.supported_signing_schemes =
        key_info.supported_signing_schemes;

    EXPECT_TRUE(KeyInfoEquals(expected_key_info, key_info));
  }

  {
    expected_key_info.nickname = "new_nickname";

    base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
    kcer_->SetKeyNickname(PrivateKeyHandle(public_key),
                          expected_key_info.nickname.value(),
                          set_nickname_waiter.GetCallback());
    ASSERT_TRUE(set_nickname_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  {
    base::test::TestFuture<
        base::expected<std::optional<chaps::KeyPermissions>, Error>>
        key_permissions_waiter;
    kcer_->GetKeyPermissions(PrivateKeyHandle(public_key),
                             key_permissions_waiter.GetCallback());
    ASSERT_TRUE(key_permissions_waiter.Get().has_value());
    const std::optional<chaps::KeyPermissions>& key_permissions =
        key_permissions_waiter.Get().value();
    EXPECT_TRUE(
        ExpectKeyPermissionsEqual(expected_key_permissions, key_permissions));
  }

  {
    expected_key_permissions.mutable_key_usages()->set_corporate(true);
    expected_key_permissions.mutable_key_usages()->set_arc(true);

    base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
    kcer_->SetKeyPermissions(PrivateKeyHandle(public_key),
                             expected_key_permissions,
                             set_permissions_waiter.GetCallback());
    ASSERT_TRUE(set_permissions_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<
        base::expected<std::optional<chaps::KeyPermissions>, Error>>
        key_permissions_waiter;
    kcer_->GetKeyPermissions(PrivateKeyHandle(public_key),
                             key_permissions_waiter.GetCallback());
    ASSERT_TRUE(key_permissions_waiter.Get().has_value());
    const std::optional<chaps::KeyPermissions>& key_permissions =
        key_permissions_waiter.Get().value();
    EXPECT_TRUE(
        ExpectKeyPermissionsEqual(expected_key_permissions, key_permissions));
  }

  {
    expected_cert_provisioning_profile_id = "cert_prov_id_123";

    base::test::TestFuture<base::expected<void, Error>> set_cert_prov_id_waiter;
    kcer_->SetCertProvisioningProfileId(PrivateKeyHandle(public_key),
                                        expected_cert_provisioning_profile_id,
                                        set_cert_prov_id_waiter.GetCallback());
    ASSERT_TRUE(set_cert_prov_id_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<std::optional<std::string>, Error>>
        cert_prov_waiter;
    kcer_->GetCertProvisioningProfileId(PrivateKeyHandle(public_key),
                                        cert_prov_waiter.GetCallback());
    ASSERT_TRUE(cert_prov_waiter.Get().has_value());
    EXPECT_EQ(expected_cert_provisioning_profile_id,
              cert_prov_waiter.Get().value());
  }

  // Check that the setters above didn't modify unrelated attributes.
  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

TEST_F(KcerNssTest, ImportCertForImportedKey) {
  InitializeKcer({Token::kUser});

  std::optional<std::vector<uint8_t>> key = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.key"));
  ASSERT_TRUE(key.has_value() && (key->size() > 0));

  std::optional<std::vector<uint8_t>> cert = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.pem"));
  ASSERT_TRUE(cert.has_value() && (cert->size() > 0));

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer_->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer(std::move(key.value())),
                   import_key_waiter.GetCallback());
  ASSERT_TRUE(import_key_waiter.Get().has_value());

  const PublicKey& public_key = import_key_waiter.Get().value();

  EXPECT_EQ(public_key.GetToken(), Token::kUser);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetPkcs11Id()->size(), 20u);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetSpki()->size(), 294u);

  base::test::TestFuture<base::expected<void, Error>> import_cert_waiter;
  kcer_->ImportCertFromBytes(Token::kUser, CertDer(std::move(cert.value())),
                             import_cert_waiter.GetCallback());
  EXPECT_TRUE(import_cert_waiter.Get().has_value());
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));

  // List certs, make sure the new cert is listed.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  const auto& certs =
      certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
  EXPECT_EQ(certs.size(), 1u);
}

// Test different ways to call DoesPrivateKeyExist() method and that it
// returns correct results when Kcer has access to one token.
TEST_F(KcerNssTest, DoesPrivateKeyExistOneToken) {
  InitializeKcer({Token::kDevice});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // The private key should be found by the PublicKey.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key.GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found on the specified token by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Looking for a key on a non-existing token should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kTokenIsNotAvailable);
  }

  // Looking for a key by an invalid SPKI should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  // Looking for a non-existing key should return a negative result.
  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

TEST_F(KcerNssTest, RemoveKeyAndCertsWithManyCerts) {
  if (NSS_VersionCheck("3.68") != PR_TRUE) {
    // TODO(b/283925148): Remove this when all the builders are updated.
    GTEST_SKIP() << "NSS is too old";
  }

  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // Import three certs, ids should be random, so they will be different.
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));
  }
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/2));
  }
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/3));
  }

  // Check that the imported cert can be found.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
            3u);

  base::test::TestFuture<base::expected<void, Error>> remove_waiter;
  kcer_->RemoveKeyAndCerts(PrivateKeyHandle(public_key),
                           remove_waiter.GetCallback());
  EXPECT_TRUE(remove_waiter.Get().has_value());
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/4));

  // Check that the imported cert cannot be found anymore.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter_2;
  kcer_->ListCerts({Token::kUser}, certs_waiter_2.GetCallback());
  EXPECT_TRUE(certs_waiter_2.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(
      certs_waiter_2.Get<std::vector<scoped_refptr<const Cert>>>().empty());

  // Check that the generated key cannot be found anymore.
  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_keys_waiter;
  kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
  ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(list_keys_waiter.Get<std::vector<PublicKey>>().empty());
}

class KcerNssAllKeyTypesTest : public KcerNssTest,
                               public testing::WithParamInterface<TestKeyType> {
 protected:
  TestKeyType GetKeyType() { return GetParam(); }

  // Requires Kcer to be initialized.
  std::optional<PublicKey> CreateKey(Token token, TestKeyType key_type) {
    base::test::TestFuture<base::expected<PublicKey, Error>> key_waiter;
    switch (key_type) {
      case TestKeyType::kRsa:
        kcer_->GenerateRsaKey(token, RsaModulusLength::k2048,
                              /*hardware_backed=*/true,
                              key_waiter.GetCallback());
        key_can_be_listed_ = true;
        key_type_ = KeyType::kRsa;
        break;
      case TestKeyType::kEcc:
        kcer_->GenerateEcKey(token, EllipticCurve::kP256,
                             /*hardware_backed=*/true,
                             key_waiter.GetCallback());
        key_can_be_listed_ = true;
        key_type_ = KeyType::kEcc;
        break;
      case TestKeyType::kImportedRsa: {
        std::optional<std::vector<uint8_t>> key_to_import =
            ReadPemFileReturnDer(
                net::GetTestCertsDirectory().AppendASCII("client_1.key"));
        kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(key_to_import.value()),
                         key_waiter.GetCallback());
        key_can_be_listed_ = false;
        key_type_ = KeyType::kRsa;
        break;
      }
      case TestKeyType::kImportedEcc: {
        std::optional<std::vector<uint8_t>> key_to_import =
            ReadPemFileReturnDer(
                net::GetTestCertsDirectory().AppendASCII("key_usage_p256.key"));

        kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(key_to_import.value()),
                         key_waiter.GetCallback());
        key_can_be_listed_ = false;
        key_type_ = KeyType::kEcc;
        break;
      }
    }
    if (!key_waiter.Get().has_value()) {
      return std::nullopt;
    }
    return key_waiter.Take().value();
  }

  SigningScheme GetSuitableSigningScheme() {
    switch (GetKeyType()) {
      case TestKeyType::kRsa:
      case TestKeyType::kImportedRsa:
        return SigningScheme::kRsaPkcs1Sha256;
      case TestKeyType::kEcc:
      case TestKeyType::kImportedEcc:
        return SigningScheme::kEcdsaSecp256r1Sha256;
    }
  }

  // TODO(miersh): The implementation of ImportKey that uses NSS is not able to
  // list imported keys (even though it can do other operations with them). This
  // should be fixed in Kcer-without-NSS.
  bool key_can_be_listed_ = false;
  KeyType key_type_ = KeyType::kRsa;
};

// Test different ways to call DoesPrivateKeyExist() method and that it
// returns correct results when Kcer has access to two tokens.
TEST_P(KcerNssAllKeyTypesTest, DoesPrivateKeyExistTwoTokens) {
  InitializeKcer({Token::kUser, Token::kDevice});
  std::optional<PublicKey> public_key = CreateKey(Token::kDevice, GetKeyType());
  ASSERT_TRUE(public_key.has_value());

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key.value()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key->GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key->GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key->GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Simulate a potential lifecycle of a key and related objects.
TEST_P(KcerNssAllKeyTypesTest, KeyLifecycle) {
  InitializeKcer({Token::kUser, Token::kDevice});

  // Check that the initialized tokens are returned as available.
  base::test::TestFuture<base::flat_set<Token>> get_tokens_waiter;
  kcer_->GetAvailableTokens(get_tokens_waiter.GetCallback());
  EXPECT_EQ(get_tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));

  // Check that the information about both initialized tokens is available.
  {
    base::test::TestFuture<base::expected<TokenInfo, Error>>
        get_token_info_waiter;
    kcer_->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
    ASSERT_TRUE(get_token_info_waiter.Get().has_value());
    const TokenInfo& token_info = get_token_info_waiter.Get().value();
    EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
    EXPECT_THAT(token_info.token_name,
                testing::StartsWith("NSS Application Slot"));
    EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
  }
  {
    base::test::TestFuture<base::expected<TokenInfo, Error>>
        get_token_info_waiter;
    kcer_->GetTokenInfo(Token::kDevice, get_token_info_waiter.GetCallback());
    ASSERT_TRUE(get_token_info_waiter.Get().has_value());
    const TokenInfo& token_info = get_token_info_waiter.Get().value();
    EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
    EXPECT_THAT(token_info.token_name,
                testing::StartsWith("NSS Application Slot"));
    EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
  }

  // Add a new key.
  std::optional<PublicKey> public_key = CreateKey(Token::kUser, GetKeyType());
  ASSERT_TRUE(public_key.has_value());

  // Check that the key is listed.
  if (key_can_be_listed_) {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_EQ(list_keys_waiter.Get<std::vector<PublicKey>>().size(), 1u);
  }

  // Check that the key exists.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key->GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Check that the key can sign data.
  {
    DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    SigningScheme signing_scheme = GetSuitableSigningScheme();
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key.value()), signing_scheme,
                data_to_sign, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value()) << sign_waiter.Get().error();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key->GetSpki(),
                                data_to_sign, sign_waiter.Get().value()));
  }

  // Check that the key can sign pre-hashed data.
  if (key_type_ == KeyType::kRsa) {
    DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    // A caller would need to hash the data themself before calling
    // `SignRsaPkcs1Digest`, do that here.
    auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hasher->Update(data_to_sign->data(), data_to_sign->size());
    std::vector<uint8_t> hash(hasher->GetHashLength());
    hasher->Finish(hash.data(), hash.size());
    DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

    // Generate the signature.
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->SignRsaPkcs1Raw(PrivateKeyHandle(public_key.value()),
                           std::move(digest_with_prefix),
                           sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    // Verify the signature.
    EXPECT_TRUE(VerifySignature(SigningScheme::kRsaPkcs1Sha256,
                                public_key->GetSpki(), data_to_sign,
                                signature));
  }

  // Import a cert for the key.
  scoped_refptr<net::X509Certificate> x509_cert_1;
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key->GetSpki().value());
    x509_cert_1 = cert_builder->GetX509Certificate();

    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, x509_cert_1,
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));
  }

  // List certs, make sure the new cert is listed.
  scoped_refptr<const Cert> kcer_cert_1;
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    const auto& certs =
        certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
    ASSERT_EQ(certs.size(), 1u);
    kcer_cert_1 = certs.front();
    EXPECT_TRUE(
        kcer_cert_1->GetX509Cert()->EqualsExcludingChain(x509_cert_1.get()));
  }

  // Remove the cert.
  {
    base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
    kcer_->RemoveCert(kcer_cert_1, remove_cert_waiter.GetCallback());
    ASSERT_TRUE(remove_cert_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/2));
    kcer_cert_1 = nullptr;
  }

  // Check that the cert cannot be found anymore.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
              0u);
  }

  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key->GetSpki().value());

    // Import another cert for the key to check that the key was not removed and
    // is still usable.
    CertDer cert_der(StrToBytes(cert_builder->GetDER()));
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportCertFromBytes(Token::kUser, std::move(cert_der),
                               import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/3));
  }

  // Check that the cert can be found.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter_3;
    kcer_->ListCerts({Token::kUser}, certs_waiter_3.GetCallback());
    EXPECT_TRUE(certs_waiter_3.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(
        certs_waiter_3.Get<std::vector<scoped_refptr<const Cert>>>().size(),
        1u);
  }

  if ((key_type_ == KeyType::kEcc) && NSS_VersionCheck("3.68") != PR_TRUE) {
    // TODO(b/283925148): Old NSS crashes on an attempt to remove an ECC key.
    // Most test running builders are up-to-date enough, but for the remaining
    // few just skip the rest of the test.
    return;
  }

  // Remove key and its certs.
  {
    base::test::TestFuture<base::expected<void, Error>> remove_waiter;
    kcer_->RemoveKeyAndCerts(PrivateKeyHandle(public_key.value()),
                             remove_waiter.GetCallback());
    EXPECT_TRUE(remove_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/4));
  }

  // Check that the cert cannot be found anymore.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
              0u);
  }

  // Check that the key is not listed anymore.
  if (key_can_be_listed_) {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_EQ(list_keys_waiter.Get<std::vector<PublicKey>>().size(), 0u);
  }
}

INSTANTIATE_TEST_SUITE_P(AllKeyTypes,
                         KcerNssAllKeyTypesTest,
                         testing::Values(TestKeyType::kRsa,
                                         TestKeyType::kEcc,
                                         TestKeyType::kImportedRsa,
                                         TestKeyType::kImportedEcc),
                         // Make test names more readable:
                         [](const auto& info) {
                           return TestKeyTypeToStr(info.param);
                         });

// These tests are different from the ones above because
// KcerTokenImplNss::ImportPkcs12Cert communicates with both NSS and Chaps, but
// it's difficult to implement a sufficiently realistic fake chaps for NSS to be
// able to work with it. So instead they mostly just cover that the requests to
// Chaps are correct and on a real device that would be enough for NSS to detect
// the imported certs and keys. Import of PKCS#12 files is also additionally
// covered by the CertSettingsPage* tast tests.
using KcerNssImportPkcs12Test = KcerNssTest;

std::vector<uint8_t> ReadTestFile(const std::string& file_name) {
  base::FilePath file_path =
      net::GetTestCertsDirectory().AppendASCII(file_name);
  std::optional<std::vector<uint8_t>> file_data =
      base::ReadFileToBytes(file_path);
  if (!file_data.has_value()) {
    ADD_FAILURE() << "Couldn't read " << file_path;
    return {};
  }
  return file_data.value();
}

const std::vector<uint8_t>& GetPkcs12DataRsa() {
  static std::vector<uint8_t> pkcs12_data = ReadTestFile("client.p12");
  return pkcs12_data;
}

const std::vector<uint8_t>& GetPkcs12DataEc() {
  static std::vector<uint8_t> pkcs12_data =
      ReadTestFile("client_with_ec_key.p12");
  return pkcs12_data;
}

const std::vector<uint8_t>& GetPkcs12DataWith2Certs() {
  static std::vector<uint8_t> pkcs12_data =
      ReadTestFile("2_client_certs_1_key.p12");
  return pkcs12_data;
}

base::flat_map<uint32_t /*attribute_id*/, const chaps::Attribute*> MakeMap(
    const chaps::AttributeList& attrs) {
  base::flat_map<uint32_t, const chaps::Attribute*> result;
  for (int i = 0; i < attrs.attributes_size(); ++i) {
    result[attrs.attributes(i).type()] = &attrs.attributes(i);
  }
  if (base::checked_cast<size_t>(attrs.attributes_size()) != result.size()) {
    ADD_FAILURE() << "Duplicate attributes detected";
  }
  return result;
}

[[nodiscard]] bool FindAttribute(chaps::AttributeList attrs,
                                 uint32_t attribute_type,
                                 base::span<const uint8_t> value) {
  for (int i = 0; i < attrs.attributes_size(); ++i) {
    const chaps::Attribute& cur_attr = attrs.attributes(i);
    if (cur_attr.type() != attribute_type) {
      continue;
    }
    // There shouldn't be two attributes with the same type and different
    // values, if this one is not the one, return false;
    if (!cur_attr.has_length() || !cur_attr.has_value()) {
      return false;
    }

    return SpanEqual(base::as_byte_span(cur_attr.value()), value);
  }
  return false;
}

// PKCS#12 import: test that importing a file with a cert and an RSA key works.
TEST_F(KcerNssImportPkcs12Test, CertWithRsaKeySuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  chaps::AttributeList find_key_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_key_attrs),
                      RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                         chromeos::PKCS11_CKR_OK)));

  chaps::AttributeList private_key_attrs;
  chaps::AttributeList public_key_attrs;
  chaps::AttributeList cert_attrs;
  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&private_key_attrs),
                RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&public_key_attrs),
                RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/true,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());

  constexpr CK_OBJECT_CLASS kPrivKeyClass = CKO_PRIVATE_KEY;
  constexpr CK_OBJECT_CLASS kPublicKeyClass = CKO_PUBLIC_KEY;
  constexpr CK_OBJECT_CLASS kCertClass = CKO_CERTIFICATE;
  constexpr CK_KEY_TYPE kKeyType = CKK_RSA;
  constexpr CK_KEY_TYPE kCertType = CKC_X_509;
  constexpr CK_BBOOL kTrue = CK_TRUE;
  constexpr CK_BBOOL kFalse = CK_FALSE;
  // At the moment of writing these key components were printed from the code
  // under test, i.e. not guaranteed to be correct. The code was also tested
  // on a real device, so most likely they are correct. Long term this is a
  // regression test.
  const std::vector<uint8_t> kModulus =
      base::Base64Decode(
          "1JC7k5aWwwOpqoiNzoRHLRdmzH9h4kVmFlBU/vZ5e7hCSnnIbVJilMxDB+p0b7ozw1/"
          "bHvsRqikARkMc0OnC4EMnm6BEopqiyOnNGBy1qXwol5Mw8T8zwlzJl7FQdQdlH7pMxuI"
          "D8hZEu8VkoEyLYJVJ1Ylaasc5BC0pHxZdNKk=")
          .value();
  const std::vector<uint8_t> kPkcs11Id =
      base::Base64Decode("U65QueEa+ljfdKySfD6QbFrXEcM=").value();
  const std::vector<uint8_t> kPublicExponent =
      base::Base64Decode("AQAB").value();
  const std::vector<uint8_t> kPrivateExponent =
      base::Base64Decode(
          "y/k2hiFy+h+BqArxSMLWKgbStlll7GL7212qsh6B5J6jviOumHj98BsyF1577"
          "NqY4VoSQmBaSxadFM9Bz5cBT8IrKr2/FjL1AC+wgdwUvGvbD426zN4Yb59cTf/"
          "bhNkvd2xocFPHeMDETFD6ISEcV6YLbPAtNlom7qVxlSTn1KE=")
          .value();
  const std::vector<uint8_t> kPrime1 =
      base::Base64Decode(
          "8W127p18wtuvUBxz7MtZgAPk/1OGLj1RJghuVYbHaCJ9sT5AzK8eNcRqCld/"
          "bKABDdmYf3QHKYDx+vcrhcNF8w==")
          .value();
  const std::vector<uint8_t> kPrime2 =
      base::Base64Decode(
          "4WVKE2h5oF7HYpX2sLgHXFhM77k6Hb1MalKk1MvXSYeKLnFf1Xh4Af2tUR73RmG/Mp/"
          "evvUMu6h4AvlGvn+18w==")
          .value();
  const std::vector<uint8_t> kExponent1 =
      base::Base64Decode(
          "SUZzCXstKaspq4PnP2B8upj0APalzBT6MzPt4PF2RknpokkFu9oOrjz9/"
          "kOOPjbV+xEm8tAReGxVhVlNkVyyNw==")
          .value();
  const std::vector<uint8_t> kExponent2 =
      base::Base64Decode(
          "DkFqwvl7n9H+yFR1ys2I4aVQEGVlsJXVbHAXrsHJtwPUkIVpK0Y4SN/"
          "zg0rzFsd94UTNQMSc7o2EMaP0fn3zUw==")
          .value();
  const std::vector<uint8_t> kCoefficient =
      base::Base64Decode(
          "mV2Q/My7RVOOsSZGDEouCYMcVahOFWS84IcpYRwR9ds0KZ4hKcdyMGNR5/"
          "4ryvr9XMA+DBR/L9GBSWe6CeK3RQ==")
          .value();
  const std::vector<uint8_t> kCertDer =
      base::Base64Decode(
          "MIICpTCCAg6gAwIBAgIBATANBgkqhkiG9w0BAQUFADBWMQswCQYDVQQGEwJBVTETMBEG"
          "A1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRk"
          "MQ8wDQYDVQQDEwZ0ZXN0Y2EwIBcNMTAwNzMwMDEwMjEyWhgPMjA2MDA3MTcwMTAyMTJa"
          "MFwxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRl"
          "cm5ldCBXaWRnaXRzIFB0eSBMdGQxFTATBgNVBAMTDHRlc3R1c2VyY2VydDCBnzANBgkq"
          "hkiG9w0BAQEFAAOBjQAwgYkCgYEA1JC7k5aWwwOpqoiNzoRHLRdmzH9h4kVmFlBU/"
          "vZ5e7hCSnnIbVJilMxDB+p0b7ozw1/"
          "bHvsRqikARkMc0OnC4EMnm6BEopqiyOnNGBy1qXwol5Mw8T8zwlzJl7FQdQdlH7pMxuI"
          "D8hZEu8VkoEyLYJVJ1Ylaasc5BC0pHxZdNKkCAwEAAaN7MHkwCQYDVR0TBAIwADAsBgl"
          "ghkgBhvhCAQ0EHxYdT3BlblNTTCBHZW5lcmF0ZWQgQ2VydGlmaWNhdGUwHQYDVR0OBBY"
          "EFHqEH18NKRVbhkqTT8swZq22Dc4YMB8GA1UdIwQYMBaAFE8aGkwMhipgaDysVMfu3Ja"
          "N29ILMA0GCSqGSIb3DQEBBQUAA4GBAKMT7cwjZtgmkFrJPAa/"
          "oOt1cdoBD7MqErx+tdvVN62q0h0Vl6UM3a94Ic0/"
          "sv1V8RT5TUYUyyuepr2Gm58uqkcbI3qflveVcvi96n7fCCo6NwxbKHmpVOx+"
          "wcPlHtjfek2KGQnee3mEN0YY/HOP5Rvj0Bh302kLrfgFx3xN1G5I")
          .value();
  const std::vector<uint8_t> kIssuerDer =
      base::Base64Decode(
          "MFYxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRl"
          "cm5l"
          "dCBXaWRnaXRzIFB0eSBMdGQxDzANBgNVBAMTBnRlc3RjYQ==")
          .value();
  const std::vector<uint8_t> kSubjectDer =
      base::Base64Decode(
          "MFwxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRl"
          "cm5l"
          "dCBXaWRnaXRzIFB0eSBMdGQxFTATBgNVBAMTDHRlc3R1c2VyY2VydA==")
          .value();
  const std::vector<uint8_t> kSerialDer = base::Base64Decode("AgEB").value();
  const std::string kNickname = "testusercert";

  {
    base::flat_map<uint32_t, const chaps::Attribute*> find_key_map =
        MakeMap(find_key_attrs);

    EXPECT_TRUE(
        SpanEqual(find_key_map[CKA_CLASS]->value(), MakeSpan(&kPrivKeyClass)));
    EXPECT_TRUE(SpanEqual(find_key_map[CKA_ID]->value(), kPkcs11Id));
  }

  {
    // Use NSS constants (e.g CKA_CLASS instead of chromeos::PKCS11_CKA_CLASS)
    // to verify that the outcome will be compatible with NSS.
    base::flat_map<uint32_t, const chaps::Attribute*> private_key_map =
        MakeMap(private_key_attrs);
    EXPECT_EQ(private_key_map.size(), 19u);
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_CLASS]->value(),
                          MakeSpan(&kPrivKeyClass)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_KEY_TYPE]->value(), MakeSpan(&kKeyType)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_SENSITIVE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_EXTRACTABLE]->value(),
                          MakeSpan(&kFalse)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_PRIVATE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_UNWRAP]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_DECRYPT]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_SIGN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_SIGN_RECOVER]->value(),
                          MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_MODULUS]->value(), kModulus));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_ID]->value(), kPkcs11Id));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_PUBLIC_EXPONENT]->value(),
                          kPublicExponent));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_PRIVATE_EXPONENT]->value(),
                          kPrivateExponent));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_PRIME_1]->value(), kPrime1));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_PRIME_2]->value(), kPrime2));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_EXPONENT_1]->value(), kExponent1));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_EXPONENT_2]->value(), kExponent2));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_COEFFICIENT]->value(), kCoefficient));
    EXPECT_FALSE(
        base::Contains(private_key_map, chaps::kForceSoftwareAttribute));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> public_key_map =
        MakeMap(public_key_attrs);
    EXPECT_EQ(public_key_map.size(), 9u);
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_CLASS]->value(),
                          MakeSpan(&kPublicKeyClass)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_KEY_TYPE]->value(), MakeSpan(&kKeyType)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_WRAP]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_ENCRYPT]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_VERIFY]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_ID]->value(), kPkcs11Id));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_MODULUS]->value(), kModulus));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_PUBLIC_EXPONENT]->value(),
                          kPublicExponent));
    EXPECT_FALSE(
        base::Contains(public_key_map, chaps::kForceSoftwareAttribute));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> cert_map =
        MakeMap(cert_attrs);
    EXPECT_EQ(cert_map.size(), 9u);
    EXPECT_TRUE(SpanEqual(cert_map[CKA_CLASS]->value(), MakeSpan(&kCertClass)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_CERTIFICATE_TYPE]->value(),
                          MakeSpan(&kCertType)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_ID]->value(), kPkcs11Id));
    EXPECT_TRUE(
        SpanEqual(cert_map[CKA_LABEL]->value(), base::as_byte_span(kNickname)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_VALUE]->value(), kCertDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_ISSUER]->value(), kIssuerDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_SUBJECT]->value(), kSubjectDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_SERIAL_NUMBER]->value(), kSerialDer));
  }
  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 1)));
  }
}

// PKCS#12 import: test that imported certs and RSA keys will be marked
// as software-backed and migrated when necessary.
TEST_F(KcerNssImportPkcs12Test, CertWithRsaKeyAndExtraArgsSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  chaps::AttributeList private_key_attrs;
  chaps::AttributeList public_key_attrs;
  chaps::AttributeList cert_attrs;
  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&private_key_attrs),
                RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&public_key_attrs),
                RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/true,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());

  constexpr CK_BBOOL kTrue = CK_TRUE;

  {
    base::flat_map<uint32_t, const chaps::Attribute*> private_key_map =
        MakeMap(private_key_attrs);
    EXPECT_EQ(private_key_map.size(), 21u);
    EXPECT_TRUE(
        SpanEqual(private_key_map[chaps::kForceSoftwareAttribute]->value(),
                  MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_EXTRACTABLE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> public_key_map =
        MakeMap(public_key_attrs);
    EXPECT_EQ(public_key_map.size(), 11u);
    EXPECT_TRUE(
        SpanEqual(public_key_map[chaps::kForceSoftwareAttribute]->value(),
                  MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(public_key_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> cert_map =
        MakeMap(cert_attrs);
    EXPECT_EQ(cert_map.size(), 11u);
    EXPECT_TRUE(SpanEqual(cert_map[chaps::kForceSoftwareAttribute]->value(),
                          MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(cert_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 1)));
  }
}

// PKCS#12 import: test that importing a file with a cert and an EC key works.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12CertEcSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  chaps::AttributeList find_key_attrs;
  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(DoAll(MoveArg<1>(&find_key_attrs),
                      RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                         chromeos::PKCS11_CKR_OK)));

  chaps::AttributeList private_key_attrs;
  chaps::AttributeList public_key_attrs;
  chaps::AttributeList cert_attrs;
  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&private_key_attrs),
                RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&public_key_attrs),
                RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataEc()),
                          kPkcs12EcFilePassword, /*hardware_backed=*/true,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());

  constexpr CK_OBJECT_CLASS kPrivKeyClass = CKO_PRIVATE_KEY;
  constexpr CK_OBJECT_CLASS kPublicKeyClass = CKO_PUBLIC_KEY;
  constexpr CK_OBJECT_CLASS kCertClass = CKO_CERTIFICATE;
  constexpr CK_KEY_TYPE kKeyType = CKK_EC;
  constexpr CK_KEY_TYPE kCertType = CKC_X_509;
  constexpr CK_BBOOL kTrue = CK_TRUE;
  constexpr CK_BBOOL kFalse = CK_FALSE;
  // At the moment of writing these key components were printed from the code
  // under test, i.e. not guaranteed to be correct. The code was also tested
  // on a real device, so most likely they are correct. Long term this is a
  // regression test.
  const std::vector<uint8_t> kEcPoint =
      base::Base64Decode(
          "BEEE/"
          "4hAEQ+bd7cAFAyFBpmUTTDyoiOfS0ofqNMR6RC+"
          "0qhSEWjaczhD1UDcwuWNUXu9ptXwK4f39SQq3YUyDaIcYw==")
          .value();
  const std::vector<uint8_t> kEcParams =
      base::Base64Decode("BggqhkjOPQMBBw==").value();
  const std::vector<uint8_t> kPkcs11Id =
      base::Base64Decode("9kVFdOhn8yYso7a/wG2uC0wdHWo=").value();
  const std::vector<uint8_t> kPrivateKeyValue =
      base::Base64Decode("fvWtrgVAq5JApBuCPK92IUAQQnnEoLUrBgZ/KGFhz7E=")
          .value();
  const std::vector<uint8_t> kCertDer =
      base::Base64Decode(
          "MIIB2zCCAX+"
          "gAwIBAgIEFhkiazAMBggqhkjOPQQDBAUAMGIxCzAJBgNVBAYTAkRFMQswCQYDVQQIEwJ"
          "CWTEMMAoGA1UEBxMDTXVjMRMwEQYDVQQKEwpDb21tZXJjaWFsMRMwEQYDVQQLEwpOZXR"
          "3b3JraW5nMQ4wDAYDVQQDEwVDTmFtZTAeFw0yMzExMDIxODQ3MzBaFw0yNDExMDExODQ"
          "3MzBaMGIxCzAJBgNVBAYTAkRFMQswCQYDVQQIEwJCWTEMMAoGA1UEBxMDTXVjMRMwEQY"
          "DVQQKEwpDb21tZXJjaWFsMRMwEQYDVQQLEwpOZXR3b3JraW5nMQ4wDAYDVQQDEwVDTmF"
          "tZTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABP+"
          "IQBEPm3e3ABQMhQaZlE0w8qIjn0tKH6jTEekQvtKoUhFo2nM4Q9VA3MLljVF7vabV8Cu"
          "H9/UkKt2FMg2iHGOjITAfMB0GA1UdDgQWBBT2RUV06GfzJiyjtr/"
          "Aba4LTB0dajAMBggqhkjOPQQDBAUAA0gAMEUCIQCn1ViT++"
          "SLXVv4sExxORcVHVGtyCp6HLVVkJW0IP+"
          "SawIgSqGQMDoVRFHq6Zel10xDQM8AB0814PJK37LXQZMjRLg=")
          .value();
  const std::vector<uint8_t> kIssuerDer =
      base::Base64Decode(
          "MGIxCzAJBgNVBAYTAkRFMQswCQYDVQQIEwJCWTEMMAoGA1UEBxMDTXVjMRMwEQYDVQQK"
          "EwpDb21tZXJjaWFsMRMwEQYDVQQLEwpOZXR3b3JraW5nMQ4wDAYDVQQDEwVDTmFtZQ="
          "=")
          .value();
  const std::vector<uint8_t> kSubjectDer =
      base::Base64Decode(
          "MGIxCzAJBgNVBAYTAkRFMQswCQYDVQQIEwJCWTEMMAoGA1UEBxMDTXVjMRMwEQYDVQQK"
          "EwpDb21tZXJjaWFsMRMwEQYDVQQLEwpOZXR3b3JraW5nMQ4wDAYDVQQDEwVDTmFtZQ="
          "=")
          .value();
  const std::vector<uint8_t> kSerialDer =
      base::Base64Decode("AgQWGSJr").value();
  const std::string kNickname = "serverkey";

  {
    base::flat_map<uint32_t, const chaps::Attribute*> find_key_map =
        MakeMap(find_key_attrs);

    EXPECT_TRUE(
        SpanEqual(find_key_map[CKA_CLASS]->value(), MakeSpan(&kPrivKeyClass)));
    EXPECT_TRUE(SpanEqual(find_key_map[CKA_ID]->value(), kPkcs11Id));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> private_key_map =
        MakeMap(private_key_attrs);
    EXPECT_EQ(private_key_map.size(), 13u);
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_CLASS]->value(),
                          MakeSpan(&kPrivKeyClass)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_KEY_TYPE]->value(), MakeSpan(&kKeyType)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_SENSITIVE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_EXTRACTABLE]->value(),
                          MakeSpan(&kFalse)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_PRIVATE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_SIGN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_SIGN_RECOVER]->value(),
                          MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_DERIVE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_ID]->value(), kPkcs11Id));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_EC_POINT]->value(), kEcPoint));
    EXPECT_TRUE(SpanEqual(private_key_map[CKA_EC_PARAMS]->value(), kEcParams));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_VALUE]->value(), kPrivateKeyValue));

    EXPECT_FALSE(
        base::Contains(private_key_map, chaps::kForceSoftwareAttribute));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> public_key_map =
        MakeMap(public_key_attrs);
    EXPECT_EQ(public_key_map.size(), 8u);
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_CLASS]->value(),
                          MakeSpan(&kPublicKeyClass)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_KEY_TYPE]->value(), MakeSpan(&kKeyType)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_VERIFY]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(public_key_map[CKA_DERIVE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_EC_PARAMS]->value(), kEcParams));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_EC_POINT]->value(), kEcPoint));
    EXPECT_TRUE(SpanEqual(public_key_map[CKA_ID]->value(), kPkcs11Id));

    EXPECT_FALSE(
        base::Contains(public_key_map, chaps::kForceSoftwareAttribute));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> cert_map =
        MakeMap(cert_attrs);
    EXPECT_EQ(cert_map.size(), 9u);
    EXPECT_TRUE(SpanEqual(cert_map[CKA_CLASS]->value(), MakeSpan(&kCertClass)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_CERTIFICATE_TYPE]->value(),
                          MakeSpan(&kCertType)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_TOKEN]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_ID]->value(), kPkcs11Id));
    EXPECT_TRUE(
        SpanEqual(cert_map[CKA_LABEL]->value(), base::as_byte_span(kNickname)));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_VALUE]->value(), kCertDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_ISSUER]->value(), kIssuerDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_SUBJECT]->value(), kSubjectDer));
    EXPECT_TRUE(SpanEqual(cert_map[CKA_SERIAL_NUMBER]->value(), kSerialDer));
  }

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcCertImportTask, 1)));
  }
}

// PKCS#12 import: test that imported certs and EC keys will be marked
// as software-backed and migrated when necessary.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12CertEcWithExtraArgsSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  chaps::AttributeList private_key_attrs;
  chaps::AttributeList public_key_attrs;
  chaps::AttributeList cert_attrs;
  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(
          DoAll(MoveArg<1>(&private_key_attrs),
                RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&public_key_attrs),
                RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataEc()),
                          kPkcs12EcFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/true,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());

  constexpr CK_BBOOL kTrue = CK_TRUE;

  {
    base::flat_map<uint32_t, const chaps::Attribute*> private_key_map =
        MakeMap(private_key_attrs);
    EXPECT_EQ(private_key_map.size(), 15u);
    EXPECT_TRUE(
        SpanEqual(private_key_map[chaps::kForceSoftwareAttribute]->value(),
                  MakeSpan(&kTrue)));
    EXPECT_TRUE(
        SpanEqual(private_key_map[CKA_EXTRACTABLE]->value(), MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(private_key_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> public_key_map =
        MakeMap(public_key_attrs);
    EXPECT_EQ(public_key_map.size(), 10u);
    EXPECT_TRUE(
        SpanEqual(public_key_map[chaps::kForceSoftwareAttribute]->value(),
                  MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(public_key_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    base::flat_map<uint32_t, const chaps::Attribute*> cert_map =
        MakeMap(cert_attrs);
    EXPECT_EQ(cert_map.size(), 11u);
    EXPECT_TRUE(SpanEqual(cert_map[chaps::kForceSoftwareAttribute]->value(),
                          MakeSpan(&kTrue)));
    EXPECT_TRUE(SpanEqual(cert_map[kCkaChromeOsMigratedFromNss]->value(),
                          MakeSpan(&kTrue)));
  }

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcCertImportTask, 1)));
  }
}

// PKCS#12 import: test that the key is not imported again if it already exists.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12KeyExistsSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{ObjectHandle(1)},
                                   chromeos::PKCS11_CKR_OK));

  chaps::AttributeList cert_attrs;
  EXPECT_CALL(chaps_client_, CreateObject)
      .WillOnce(
          DoAll(MoveArg<1>(&cert_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());
  constexpr CK_OBJECT_CLASS kCertClass = CKO_CERTIFICATE;
  EXPECT_TRUE(FindAttribute(cert_attrs, CKA_CLASS, MakeSpan(&kCertClass)));

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 1)));
  }
}

// PKCS#12 import: test that the import fails correctly when Chaps fails to
// check whether the key already exists.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12FailToCheckKeyExists) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToSearchForObjects);

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import is retried correctly when Chaps fails to
// check whether the key already exists with a session error.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12RetryToCheckKeyExists) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>(), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kPkcs11SessionFailure);

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import fails correctly when Chaps fails to
// create private key.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12FailToCreatePrivKey) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, CreateObject)
      .WillOnce(RunOnceCallback<2>(ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToImportKey);
  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import is retried correctly when Chaps fails to
// create private key with a session error.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12RetryToCreatePrivKey) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(std::vector<ObjectHandle>(),
                                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          ObjectHandle(0), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kPkcs11SessionFailure);
  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import fails correctly when Chaps fails to
// create private key.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12FailToCreatePubKey) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  ObjectHandle priv_key_handle(10);
  EXPECT_CALL(chaps_client_, CreateObject)
      .WillOnce(RunOnceCallback<2>(priv_key_handle, chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(chaps_client_,
              DestroyObjectsWithRetries(
                  slot_id, std::vector<ObjectHandle>{priv_key_handle}, _))
      .WillOnce(RunOnceCallback<2>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataRsa()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToImportKey);
  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import fails correctly when Chaps fails to
// create a cert.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12FailToCreateCert) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(3),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataEc()),
                          kPkcs12EcFilePassword, /*hardware_backed=*/true,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  EXPECT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToImportCertificate);
  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcCertImportTask, 0)));
  }
}

// PKCS#12 import: test that the import is retried correctly when Chaps fails to
// create a cert with a session error.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12RetryToCreateCert) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  // Simulate that the key is found to skip its creation for simplicity.
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{ObjectHandle(1)},
                                   chromeos::PKCS11_CKR_OK));

  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(kDefaultAttempts)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          ObjectHandle(3), chromeos::PKCS11_CKR_SESSION_CLOSED));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataEc()),
                          kPkcs12EcFilePassword, /*hardware_backed=*/true,
                          /*mark_as_migrated=*/false,
                          import_waiter.GetCallback());

  EXPECT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kPkcs11SessionFailure);

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         6),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcCertImportTask, 0)));
  }
}

// PKCS#12 import: test that importing a file with two cert and a key works.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12With2CertsSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  chaps::AttributeList cert_1_attrs;
  chaps::AttributeList cert_2_attrs;
  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_1_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)))
      .WillOnce(
          DoAll(MoveArg<1>(&cert_2_attrs),
                RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataWith2Certs()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/true,
                          import_waiter.GetCallback());

  EXPECT_TRUE(import_waiter.Get().has_value());

  base::flat_map<uint32_t, const chaps::Attribute*> cert_map_1 =
      MakeMap(cert_1_attrs);
  base::flat_map<uint32_t, const chaps::Attribute*> cert_map_2 =
      MakeMap(cert_2_attrs);
  EXPECT_EQ(cert_map_1.size(), 11u);
  EXPECT_EQ(cert_map_2.size(), 11u);

  constexpr CK_OBJECT_CLASS kCertClass = CKO_CERTIFICATE;
  EXPECT_TRUE(SpanEqual(cert_map_1[CKA_CLASS]->value(), MakeSpan(&kCertClass)));
  EXPECT_TRUE(SpanEqual(cert_map_2[CKA_CLASS]->value(), MakeSpan(&kCertClass)));
  // Check that two different certs were created.
  EXPECT_NE(cert_map_1[CKA_VALUE]->value(), cert_map_2[CKA_VALUE]->value());

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 1)));
  }
}

// PKCS#12 import: test that Kcer tries to import as many certs as possible and
// returns an error if at least one failed.
TEST_F(KcerNssImportPkcs12Test, ImportPkcs12With2CertsSemiSuccess) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>(),
                                   chromeos::PKCS11_CKR_OK));

  EXPECT_CALL(chaps_client_, CreateObject(slot_id, _, _))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(1), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(2), chromeos::PKCS11_CKR_OK))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(0),
                                   chromeos::PKCS11_CKR_GENERAL_ERROR))
      .WillOnce(RunOnceCallback<2>(ObjectHandle(3), chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataWith2Certs()),
                          kPkcs12RsaFilePassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/true,
                          import_waiter.GetCallback());

  EXPECT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kFailedToImportCertificate);

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),

            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaKeyImportTask, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessRsaCertImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedMultipleCertImport,
                         1)));
  }
}

// PKCS#12 import: test that Kcer return the correct error when a wrong password
// for a PKCS#12 file is provided.
TEST_F(KcerNssImportPkcs12Test, WrongPassword) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  const char kWrongPassword[] = "00000000";
  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportPkcs12Cert(Token::kUser, Pkcs12Blob(GetPkcs12DataWith2Certs()),
                          kWrongPassword, /*hardware_backed=*/false,
                          /*mark_as_migrated=*/true,
                          import_waiter.GetCallback());

  EXPECT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kPkcs12WrongPassword);

  {
    // Check UMA metrics recorded.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kcer::internal::KcerPkcs12ImportMetrics),
        BucketsInclude(
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport, 1),
            base::Bucket(KcerPkcs12ImportEvent::SuccessPkcs12ChapsImport, 0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImportTask,
                         0),
            base::Bucket(KcerPkcs12ImportEvent::AttemptedRsaKeyImportTask, 0),
            base::Bucket(KcerPkcs12ImportEvent::SuccessEcKeyImportTask, 0)));
  }
}

// PKCS#12 import: test that Kcer correctly handles files with empty passwords.
// An "empty" password can mean a a zero length password or no password.
TEST_F(KcerNssImportPkcs12Test, EmptyPassword) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>{ObjectHandle(1)}, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, CreateObject)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(ObjectHandle(1),
                                                   chromeos::PKCS11_CKR_OK));

  const char kEmptyPassword[] = "";

  {
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportPkcs12Cert(
        Token::kUser, Pkcs12Blob(ReadTestFile("client-null-password.p12")),
        kEmptyPassword, /*hardware_backed=*/false,
        /*mark_as_migrated=*/true, import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportPkcs12Cert(
        Token::kUser, Pkcs12Blob(ReadTestFile("client-empty-password.p12")),
        kEmptyPassword, /*hardware_backed=*/false,
        /*mark_as_migrated=*/true, import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
  }
}

// PKCS#12 import: test that Kcer correctly handles files with passwords that
// contain wide characters.
TEST_F(KcerNssImportPkcs12Test, NonAsciiPassword) {
  InitializeKcer({Token::kUser});
  SlotId slot_id(user_token_->GetSlotId());

  EXPECT_CALL(chaps_client_, FindObjects)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          std::vector<ObjectHandle>{ObjectHandle(1)}, chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, CreateObject)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(ObjectHandle(1),
                                                   chromeos::PKCS11_CKR_OK));

  std::vector<uint8_t> pkcs12_data = ReadTestFile("client_1_u16_password.p12");

  // Incorrect password should be rejected.
  {
    const std::string kNonAsciiPassword = "Wrong Password, Hello, 世界";
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportPkcs12Cert(
        Token::kUser, Pkcs12Blob(pkcs12_data), kNonAsciiPassword,
        /*hardware_backed=*/false,
        /*mark_as_migrated=*/true, import_waiter.GetCallback());
    EXPECT_FALSE(import_waiter.Get().has_value());
  }

  // Correct password should be accepted.
  {
    const std::string kNonAsciiPassword = "Hello, 世界";
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportPkcs12Cert(
        Token::kUser, Pkcs12Blob(pkcs12_data), kNonAsciiPassword,
        /*hardware_backed=*/false,
        /*mark_as_migrated=*/true, import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
  }

  // Chrome currently converts a std::u16string into std::string at some point
  // using the helper from base::, double check that the helper works as
  // expected.
  {
    const std::u16string kUtf16NonAsciiPassword = u"Hello, 世界";
    const std::string kConvertedPassword =
        base::UTF16ToUTF8(kUtf16NonAsciiPassword);
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportPkcs12Cert(
        Token::kUser, Pkcs12Blob(pkcs12_data), kConvertedPassword,
        /*hardware_backed=*/false,
        /*mark_as_migrated=*/true, import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
  }
}

}  // namespace
}  // namespace kcer
