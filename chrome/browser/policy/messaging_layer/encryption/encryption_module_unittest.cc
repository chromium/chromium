// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/policy/messaging_layer/encryption/decryption.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//       base::OnceCallback<void(ResType* res)> type which also may perform
//       some other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//                      // collected result.
//
// Or, when the callback is not expected to be invoked:
//
//   TestEvent<ResType> e(/*expected_to_complete=*/false);
//   ... Start work passing e.cb() as a completion callback,
//       which will not happen.
//
template <typename ResType>
class TestEvent {
 public:
  explicit TestEvent(bool expected_to_complete = true)
      : expected_to_complete_(expected_to_complete),
        completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~TestEvent() {
    if (expected_to_complete_) {
      EXPECT_TRUE(completed_.IsSignaled()) << "Not responded";
    } else {
      EXPECT_FALSE(completed_.IsSignaled()) << "Responded";
    }
  }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    completed_.Wait();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    DCHECK(!completed_.IsSignaled());
    return base::BindOnce(
        [](base::WaitableEvent* completed, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          completed->Signal();
        },
        base::Unretained(&completed_), base::Unretained(&result_));
  }

 private:
  bool expected_to_complete_;
  base::WaitableEvent completed_;
  ResType result_;
};

class EncryptionModuleTest : public ::testing::Test {
 protected:
  EncryptionModuleTest() = default;

  void SetUp() override {
    // Enable encryption.
    scoped_feature_list_.InitFromCommandLine(
        {EncryptionModule::kEncryptedReporting}, {});

    encryption_module_ = base::MakeRefCounted<EncryptionModule>();

    auto decryptor_result = Decryptor::Create();
    ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
    decryptor_ = std::move(decryptor_result.ValueOrDie());
  }

  StatusOr<EncryptedRecord> EncryptSync(base::StringPiece data) {
    TestEvent<StatusOr<EncryptedRecord>> encrypt_record;
    encryption_module_->EncryptRecord(data, encrypt_record.cb());
    return encrypt_record.result();
  }

  StatusOr<std::string> DecryptSync(
      std::pair<std::string /*shared_secret*/, std::string /*encrypted_data*/>
          encrypted) {
    TestEvent<StatusOr<Decryptor::Handle*>> open_decrypt;
    decryptor_->OpenRecord(encrypted.first, open_decrypt.cb());
    auto open_decrypt_result = open_decrypt.result();
    RETURN_IF_ERROR(open_decrypt_result.status());
    Decryptor::Handle* const dec_handle = open_decrypt_result.ValueOrDie();

    TestEvent<Status> add_decrypt;
    dec_handle->AddToRecord(encrypted.second, add_decrypt.cb());
    RETURN_IF_ERROR(add_decrypt.result());

    std::string decrypted_string;
    TestEvent<Status> close_decrypt;
    dec_handle->CloseRecord(base::BindOnce(
        [](std::string* decrypted_string,
           base::OnceCallback<void(Status)> close_cb,
           StatusOr<base::StringPiece> result) {
          if (!result.ok()) {
            std::move(close_cb).Run(result.status());
            return;
          }
          *decrypted_string = std::string(result.ValueOrDie());
          std::move(close_cb).Run(Status::StatusOK());
        },
        base::Unretained(&decrypted_string), close_decrypt.cb()));
    RETURN_IF_ERROR(close_decrypt.result());
    return decrypted_string;
  }

  StatusOr<std::string> DecryptMatchingSecret(uint32_t public_key_id,
                                              base::StringPiece encrypted_key) {
    // Retrieve private key that matches public key hash.
    TestEvent<StatusOr<std::string>> retrieve_private_key;
    decryptor_->RetrieveMatchingPrivateKey(public_key_id,
                                           retrieve_private_key.cb());
    ASSIGN_OR_RETURN(std::string private_key, retrieve_private_key.result());
    // Decrypt symmetric key with that private key and peer public key.
    ASSIGN_OR_RETURN(std::string shared_secret,
                     decryptor_->DecryptSecret(private_key, encrypted_key));
    return shared_secret;
  }

  Status AddNewKeyPair() {
    // Generate new pair of private key and public value.
    uint8_t out_public_value[X25519_PUBLIC_VALUE_LEN];
    uint8_t out_private_key[X25519_PRIVATE_KEY_LEN];
    X25519_keypair(out_public_value, out_private_key);

    TestEvent<Status> record_keys;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(out_private_key),
                    X25519_PRIVATE_KEY_LEN),
        std::string(reinterpret_cast<const char*>(out_public_value),
                    X25519_PUBLIC_VALUE_LEN),
        record_keys.cb());
    RETURN_IF_ERROR(record_keys.result());
    TestEvent<Status> set_public_key;
    encryption_module_->UpdateAsymmetricKey(
        std::string(reinterpret_cast<const char*>(out_public_value),
                    X25519_PUBLIC_VALUE_LEN),
        set_public_key.cb());
    RETURN_IF_ERROR(set_public_key.result());
    return Status::StatusOK();
  }

  scoped_refptr<EncryptionModule> encryption_module_;
  scoped_refptr<Decryptor> decryptor_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(EncryptionModuleTest, EncryptAndDecrypt) {
  constexpr char kTestString[] = "ABCDEF";

  // Register new pair of private key and public value.
  ASSERT_OK(AddNewKeyPair());

  // Encrypt the test string using the last public value.
  const auto encrypted_result = EncryptSync(kTestString);
  ASSERT_OK(encrypted_result.status()) << encrypted_result.status();

  // Decrypt shared secret with private asymmetric key.
  auto decrypt_secret_result = DecryptMatchingSecret(
      encrypted_result.ValueOrDie().encryption_info().public_key_id(),
      encrypted_result.ValueOrDie().encryption_info().encryption_key());
  ASSERT_OK(decrypt_secret_result.status()) << decrypt_secret_result.status();

  // Decrypt back.
  const auto decrypted_result = DecryptSync(
      std::make_pair(decrypt_secret_result.ValueOrDie(),
                     encrypted_result.ValueOrDie().encrypted_wrapped_record()));
  ASSERT_OK(decrypted_result.status()) << decrypted_result.status();

  EXPECT_THAT(decrypted_result.ValueOrDie(), ::testing::StrEq(kTestString));
}

TEST_F(EncryptionModuleTest, EncryptionDisabled) {
  constexpr char kTestString[] = "ABCDEF";

  // Disable encryption.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      {}, {EncryptionModule::kEncryptedReporting});

  // Encrypt the test string.
  const auto encrypted_result = EncryptSync(kTestString);
  ASSERT_OK(encrypted_result.status());

  // Expect the result to be identical to the original record,
  // and have no encryption_info.
  EXPECT_EQ(encrypted_result.ValueOrDie().encrypted_wrapped_record(),
            kTestString);
  EXPECT_FALSE(encrypted_result.ValueOrDie().has_encryption_info());
}

TEST_F(EncryptionModuleTest, NoPublicKey) {
  constexpr char kTestString[] = "ABCDEF";

  // Attempt to encrypt the test string.
  const auto encrypted_result = EncryptSync(kTestString);
  EXPECT_EQ(encrypted_result.status().error_code(), error::NOT_FOUND);
}

TEST_F(EncryptionModuleTest, EncryptAndDecryptMultiple) {
  constexpr const char* kTestStrings[] = {"Rec1",    "Rec22",    "Rec333",
                                          "Rec4444", "Rec55555", "Rec666666"};
  // Encrypted records.
  std::vector<EncryptedRecord> encrypted_records;

  // 1. Register first key pair.
  ASSERT_OK(AddNewKeyPair());

  // 2. Encrypt 3 test strings.
  for (const char* test_string :
       {kTestStrings[0], kTestStrings[1], kTestStrings[2]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // 3. Register second key pair.
  ASSERT_OK(AddNewKeyPair());

  // 4. Encrypt 2 test strings.
  for (const char* test_string : {kTestStrings[3], kTestStrings[4]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // 3. Register third key pair.
  ASSERT_OK(AddNewKeyPair());

  // 4. Encrypt one more test strings.
  for (const char* test_string : {kTestStrings[5]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // For every encrypted record:
  for (size_t i = 0; i < encrypted_records.size(); ++i) {
    // Decrypt encrypted_key with private asymmetric key.
    auto decrypt_secret_result = DecryptMatchingSecret(
        encrypted_records[i].encryption_info().public_key_id(),
        encrypted_records[i].encryption_info().encryption_key());
    ASSERT_OK(decrypt_secret_result.status()) << decrypt_secret_result.status();

    // Decrypt back.
    const auto decrypted_result = DecryptSync(
        std::make_pair(decrypt_secret_result.ValueOrDie(),
                       encrypted_records[i].encrypted_wrapped_record()));
    ASSERT_OK(decrypted_result.status()) << decrypted_result.status();

    // Verify match.
    EXPECT_THAT(decrypted_result.ValueOrDie(),
                ::testing::StrEq(kTestStrings[i]));
  }
}

TEST_F(EncryptionModuleTest, EncryptAndDecryptMultipleParallel) {
  // Context of single encryption. Self-destructs upon completion or failure.
  class SingleEncryptionContext {
   public:
    SingleEncryptionContext(
        base::StringPiece test_string,
        base::StringPiece public_key,
        scoped_refptr<EncryptionModule> encryption_module,
        base::OnceCallback<void(StatusOr<EncryptedRecord>)> response)
        : test_string_(test_string),
          public_key_(public_key),
          encryption_module_(encryption_module),
          response_(std::move(response)) {}

    SingleEncryptionContext(const SingleEncryptionContext& other) = delete;
    SingleEncryptionContext& operator=(const SingleEncryptionContext& other) =
        delete;

    ~SingleEncryptionContext() {
      DCHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&SingleEncryptionContext::SetPublicKey,
                                    base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<EncryptedRecord> result) {
      std::move(response_).Run(result);
      delete this;
    }
    void SetPublicKey() {
      encryption_module_->UpdateAsymmetricKey(
          public_key_,
          base::BindOnce(
              [](SingleEncryptionContext* self, Status status) {
                if (!status.ok()) {
                  self->Respond(status);
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::EncryptRecord,
                                   base::Unretained(self)));
              },
              base::Unretained(this)));
    }
    void EncryptRecord() {
      encryption_module_->EncryptRecord(
          test_string_,
          base::BindOnce(
              [](SingleEncryptionContext* self,
                 StatusOr<EncryptedRecord> encryption_result) {
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::Respond,
                                   base::Unretained(self), encryption_result));
              },
              base::Unretained(this)));
    }

   private:
    const std::string test_string_;
    const std::string public_key_;
    const scoped_refptr<EncryptionModule> encryption_module_;
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> response_;
  };

  // Context of single decryption. Self-destructs upon completion or failure.
  class SingleDecryptionContext {
   public:
    SingleDecryptionContext(
        const EncryptedRecord& encrypted_record,
        scoped_refptr<Decryptor> decryptor,
        base::OnceCallback<void(StatusOr<base::StringPiece>)> response)
        : encrypted_record_(encrypted_record),
          decryptor_(decryptor),
          response_(std::move(response)) {}

    SingleDecryptionContext(const SingleDecryptionContext& other) = delete;
    SingleDecryptionContext& operator=(const SingleDecryptionContext& other) =
        delete;

    ~SingleDecryptionContext() {
      DCHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(&SingleDecryptionContext::RetrieveMatchingPrivateKey,
                         base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<base::StringPiece> result) {
      std::move(response_).Run(result);
      delete this;
    }

    void RetrieveMatchingPrivateKey() {
      // Retrieve private key that matches public key hash.
      decryptor_->RetrieveMatchingPrivateKey(
          encrypted_record_.encryption_info().public_key_id(),
          base::BindOnce(
              [](SingleDecryptionContext* self,
                 StatusOr<std::string> private_key_result) {
                if (!private_key_result.ok()) {
                  self->Respond(private_key_result.status());
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SingleDecryptionContext::DecryptSharedSecret,
                        base::Unretained(self),
                        private_key_result.ValueOrDie()));
              },
              base::Unretained(this)));
    }

    void DecryptSharedSecret(base::StringPiece private_key) {
      // Decrypt shared secret from private key and peer public key.
      auto shared_secret_result = decryptor_->DecryptSecret(
          private_key, encrypted_record_.encryption_info().encryption_key());
      if (!shared_secret_result.ok()) {
        Respond(shared_secret_result.status());
        return;
      }
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&SingleDecryptionContext::OpenRecord,
                                    base::Unretained(this),
                                    shared_secret_result.ValueOrDie()));
    }

    void OpenRecord(base::StringPiece shared_secret) {
      decryptor_->OpenRecord(
          shared_secret,
          base::BindOnce(
              [](SingleDecryptionContext* self,
                 StatusOr<Decryptor::Handle*> handle_result) {
                if (!handle_result.ok()) {
                  self->Respond(handle_result.status());
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SingleDecryptionContext::AddToRecord,
                        base::Unretained(self),
                        base::Unretained(handle_result.ValueOrDie())));
              },
              base::Unretained(this)));
    }

    void AddToRecord(Decryptor::Handle* handle) {
      handle->AddToRecord(
          encrypted_record_.encrypted_wrapped_record(),
          base::BindOnce(
              [](SingleDecryptionContext* self, Decryptor::Handle* handle,
                 Status status) {
                if (!status.ok()) {
                  self->Respond(status);
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleDecryptionContext::CloseRecord,
                                   base::Unretained(self),
                                   base::Unretained(handle)));
              },
              base::Unretained(this), base::Unretained(handle)));
    }

    void CloseRecord(Decryptor::Handle* handle) {
      handle->CloseRecord(base::BindOnce(
          [](SingleDecryptionContext* self,
             StatusOr<base::StringPiece> decryption_result) {
            self->Respond(decryption_result);
          },
          base::Unretained(this)));
    }

   private:
    const EncryptedRecord encrypted_record_;
    const scoped_refptr<Decryptor> decryptor_;
    base::OnceCallback<void(StatusOr<base::StringPiece>)> response_;
  };

  constexpr std::array<const char*, 6> kTestStrings = {
      "Rec1", "Rec22", "Rec333", "Rec4444", "Rec55555", "Rec666666"};

  // Public and private key pairs in this test are reversed strings.
  std::vector<std::string> private_key_strings;
  std::vector<std::string> public_value_strings;
  for (size_t i = 0; i < 3; ++i) {
    // Generate new pair of private key and public value.
    uint8_t out_public_value[X25519_PUBLIC_VALUE_LEN];
    uint8_t out_private_key[X25519_PRIVATE_KEY_LEN];
    X25519_keypair(out_public_value, out_private_key);
    private_key_strings.emplace_back(
        reinterpret_cast<const char*>(out_private_key), X25519_PRIVATE_KEY_LEN);
    public_value_strings.emplace_back(
        reinterpret_cast<const char*>(out_public_value),
        X25519_PUBLIC_VALUE_LEN);
  }

  // Encrypt all records in parallel.
  std::vector<TestEvent<StatusOr<EncryptedRecord>>> results(
      kTestStrings.size());
  for (size_t i = 0; i < kTestStrings.size(); ++i) {
    // Choose random key pair.
    size_t i_key_pair = base::RandInt(0, public_value_strings.size() - 1);
    (new SingleEncryptionContext(kTestStrings[i],
                                 public_value_strings[i_key_pair],
                                 encryption_module_, results[i].cb()))
        ->Start();
  }

  // Register all key pairs for decryption.
  std::vector<TestEvent<Status>> record_results(public_value_strings.size());
  for (size_t i = 0; i < public_value_strings.size(); ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::StringPiece private_key_string,
                          base::StringPiece public_key_string,
                          scoped_refptr<Decryptor> decryptor,
                          base::OnceCallback<void(Status)> done_cb) {
                         decryptor->RecordKeyPair(private_key_string,
                                                  public_key_string,
                                                  std::move(done_cb));
                       },
                       private_key_strings[i], public_value_strings[i],
                       decryptor_, record_results[i].cb()));
  }
  // Verify registration success.
  for (auto& record_result : record_results) {
    ASSERT_OK(record_result.result()) << record_result.result();
  }

  // Decrypt all records in parallel.
  std::vector<TestEvent<StatusOr<std::string>>> decryption_results(
      kTestStrings.size());
  for (size_t i = 0; i < results.size(); ++i) {
    // Verify encryption success.
    const auto result = results[i].result();
    ASSERT_OK(result.status()) << result.status();
    // Decrypt and compare encrypted_record.
    (new SingleDecryptionContext(
         result.ValueOrDie(), decryptor_,
         base::BindOnce(
             [](base::OnceCallback<void(StatusOr<std::string>)>
                    decryption_result,
                StatusOr<base::StringPiece> result) {
               if (!result.ok()) {
                 std::move(decryption_result).Run(result.status());
                 return;
               }
               std::move(decryption_result)
                   .Run(std::string(result.ValueOrDie()));
             },
             decryption_results[i].cb())))
        ->Start();
  }

  // Verify decryption results.
  for (size_t i = 0; i < decryption_results.size(); ++i) {
    const auto decryption_result = decryption_results[i].result();
    ASSERT_OK(decryption_result.status()) << decryption_result.status();
    // Verify data match.
    EXPECT_THAT(decryption_result.ValueOrDie(),
                ::testing::StrEq(kTestStrings[i]));
  }
}

}  // namespace
}  // namespace reporting
