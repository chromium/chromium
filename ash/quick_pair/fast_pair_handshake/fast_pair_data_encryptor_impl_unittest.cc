// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::array<uint8_t, kBlockSizeBytes> kResponseBytes = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

const std::array<uint8_t, kBlockSizeBytes> kPasskeyBytes = {
    0x02, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

const std::vector<uint8_t> kAccountKey = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                          0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                          0x61, 0xC3, 0x32, 0x1D};

const char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
const char kInvalidPublicAntiSpoof[] = "";

constexpr char kValidModelId[] = "718c17";
constexpr char kTestAddress[] = "test_address";

}  // namespace

namespace ash {
namespace quick_pair {

// This controls the protocol we use to test.
using TestParam = bool;

class FastPairDataEncryptorImplTest : public testing::TestWithParam<TestParam> {
 public:
  void SetUp() override {
    data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_environment_.GetMainThreadTaskRunner());

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());
  }

  void TearDown() override { data_encryptor_.reset(); }

  void FailedSetUpNoMetadata() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    // Not using the param here to control the type of device because only
    // the kFastPairInitial protocol can fail.
    device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                           Protocol::kFastPairInitial);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device_, base::BindOnce(
                     &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr()));
  }

  void SuccessfulSetUp() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    nearby::fastpair::Device metadata;

    std::string decoded_key;
    base::Base64Decode(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    // The param controls which protocol we use.
    if (GetParam()) {
      device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                             Protocol::kFastPairInitial);
    } else {
      device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                             Protocol::kFastPairSubsequent);
      device_->set_account_key(kAccountKey);
    }

    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device_, base::BindOnce(
                     &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr()));
  }

  void SuccessfulSetUpToTestPublicKey() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    nearby::fastpair::Device metadata;

    std::string decoded_key;
    base::Base64Decode(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    // Not using the param here to control the type of device because the
    // public key expectations differ for protocols
    device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                           Protocol::kFastPairInitial);

    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device_, base::BindOnce(
                     &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr()));
  }

  void FailedSetUpNoKeyPair() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    nearby::fastpair::Device metadata;

    std::string decoded_key;
    base::Base64Decode(kInvalidPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    // Not using the param here to control the type of device because only
    // the kFastPairInitial protocol can fail..
    device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                           Protocol::kFastPairInitial);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device_, base::BindOnce(
                     &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr()));
  }

  void OnDataEncryptorCreateAsync(
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
    data_encryptor_ = std::move(fast_pair_data_encryptor);
  }

  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes() {
    return data_encryptor_->EncryptBytes(kResponseBytes);
  }

  void ParseDecryptedResponse() {
    const std::array<uint8_t, kBlockSizeBytes> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);

    data_encryptor_->ParseDecryptedResponse(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        base::BindOnce(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void ParseDecryptedResponseInvalidBytes() {
    const std::array<uint8_t, kBlockSizeBytes> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);

    data_encryptor_->ParseDecryptedResponse(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        base::BindOnce(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void ParseDecryptedResponseCallback(
      const absl::optional<DecryptedResponse>& response) {
    response_ = response;
  }

  void ParseDecryptedPasskey() {
    const std::array<uint8_t, kBlockSizeBytes> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);

    data_encryptor_->ParseDecryptedPasskey(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        base::BindOnce(
            &FastPairDataEncryptorImplTest::ParseDecryptedPasskeyCallback,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void ParseDecryptedPasskeyInvalidBytes() {
    const std::array<uint8_t, kBlockSizeBytes> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);

    data_encryptor_->ParseDecryptedPasskey(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        base::BindOnce(
            &FastPairDataEncryptorImplTest::ParseDecryptedPasskeyCallback,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void ParseDecryptedPasskeyCallback(
      const absl::optional<DecryptedPasskey>& passkey) {
    passkey_ = passkey;
  }

 protected:
  std::unique_ptr<FastPairDataEncryptor> data_encryptor_;
  absl::optional<DecryptedResponse> response_ = absl::nullopt;
  absl::optional<DecryptedPasskey> passkey_ = absl::nullopt;
  std::unique_ptr<MockQuickPairProcessManager> process_manager_;
  mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
      data_parser_remote_;
  mojo::PendingRemote<ash::quick_pair::mojom::FastPairDataParser>
      fast_pair_data_parser_;
  std::unique_ptr<ash::quick_pair::FastPairDataParser> data_parser_;

 private:
  scoped_refptr<Device> device_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FastPairDataEncryptorImplTest> weak_ptr_factory_{this};
};

TEST_P(FastPairDataEncryptorImplTest, FailedSetUpNoMetadata) {
  EXPECT_FALSE(data_encryptor_);
  FailedSetUpNoMetadata();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_encryptor_);
}

TEST_P(FastPairDataEncryptorImplTest, SuccessfulSetUp) {
  EXPECT_FALSE(data_encryptor_);
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
}

TEST_P(FastPairDataEncryptorImplTest, EncryptBytes) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_FALSE(EncryptBytes().empty());
}

TEST_P(FastPairDataEncryptorImplTest, ParseDecryptedResponse) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference);
  ParseDecryptedResponse();
  base::RunLoop().RunUntilIdle();
}

TEST_P(FastPairDataEncryptorImplTest, ParseDecryptedPasskey) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference);
  ParseDecryptedPasskey();
  base::RunLoop().RunUntilIdle();
}

TEST_P(FastPairDataEncryptorImplTest, ParseDecryptedPasskey_InvalidInputSize) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference).Times(0);
  ParseDecryptedPasskeyInvalidBytes();
  base::RunLoop().RunUntilIdle();
}

TEST_P(FastPairDataEncryptorImplTest, ParseDecryptedResponse_InvalidInputSize) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference).Times(0);
  ParseDecryptedResponseInvalidBytes();
  base::RunLoop().RunUntilIdle();
}

TEST_P(FastPairDataEncryptorImplTest, NoKeyPair) {
  FailedSetUpNoKeyPair();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_encryptor_);
}

// TODO(crbug.com/1298377) flaky on ASan + LSan bots
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_ParseDecryptedPasskey_ProcessStopped \
  DISABLED_ParseDecryptedPasskey_ProcessStopped
#else
#define MAYBE_ParseDecryptedPasskey_ProcessStopped \
  ParseDecryptedPasskey_ProcessStopped
#endif
TEST_P(FastPairDataEncryptorImplTest,
       MAYBE_ParseDecryptedPasskey_ProcessStopped) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            std::move(callback).Run(
                QuickPairProcessManager::ShutdownReason::kCrash);
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  ParseDecryptedPasskey();
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/1298377) flaky on ASan + LSan bots
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_ParseDecryptedResponse_ProcessStopped \
  DISABLED_ParseDecryptedResponse_ProcessStopped
#else
#define MAYBE_ParseDecryptedResponse_ProcessStopped \
  ParseDecryptedResponse_ProcessStopped
#endif
TEST_P(FastPairDataEncryptorImplTest,
       MAYBE_ParseDecryptedResponse_ProcessStopped) {
  SuccessfulSetUp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference)
      .WillRepeatedly(
          [&](QuickPairProcessManager::ProcessStoppedCallback callback) {
            std::move(callback).Run(
                QuickPairProcessManager::ShutdownReason::kCrash);
            return std::make_unique<
                QuickPairProcessManagerImpl::ProcessReferenceImpl>(
                data_parser_remote_, base::DoNothing());
          });
  ParseDecryptedResponse();
  base::RunLoop().RunUntilIdle();
}

TEST_P(FastPairDataEncryptorImplTest, GetPublicKey) {
  SuccessfulSetUpToTestPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_CALL(*process_manager_, GetProcessReference);
  ParseDecryptedPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(data_encryptor_->GetPublicKey(), absl::nullopt);
}

INSTANTIATE_TEST_SUITE_P(FastPairDataEncryptorImplTest,
                         FastPairDataEncryptorImplTest,
                         testing::Bool());

}  // namespace quick_pair
}  // namespace ash
