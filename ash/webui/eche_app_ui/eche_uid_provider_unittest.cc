// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_uid_provider.h"

#include "base/base64.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

class FakeExchangerClient : public mojom::UidGenerator {
 public:
  FakeExchangerClient() = default;
  ~FakeExchangerClient() override = default;

  mojo::PendingReceiver<mojom::UidGenerator> CreatePendingReceiver() {
    return remote_.BindNewPipeAndPassReceiver();
  }

  // mojom::UidGenerator:
  void GetUid(base::OnceCallback<void(const std::string&)> callback) override {
    remote_->GetUid(std::move(callback));
  }

 private:
  mojo::Remote<mojom::UidGenerator> remote_;
};

class EcheUidProviderTest : public testing::Test {
 protected:
  EcheUidProviderTest() = default;
  EcheUidProviderTest(const EcheUidProviderTest&) = delete;
  EcheUidProviderTest& operator=(const EcheUidProviderTest&) = delete;
  ~EcheUidProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(kEcheAppSeedPref, "");
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void TearDown() override {
    uid_provider_.reset();
  }
  void ResetPrefString(const std::string& path, const std::string& value) {
    pref_service_.SetString(path, value);
    uid_provider_.reset();
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void ResetUidProvider() {
    uid_provider_.reset();
    uid_provider_ = std::make_unique<EcheUidProvider>(&pref_service_);
  }
  void ClearPref(const std::string& path) {
    pref_service_.ClearPref(path);
  }
  std::string GetUid() {
    base::test::TestFuture<const std::string&> future;
    uid_provider_->GetUid(future.GetCallback());
    return future.Get();
  }
  std::optional<std::vector<uint8_t>> DecodeStringWithSeed(
      size_t expected_len) {
    std::string pref_seed = pref_service_.GetString(kEcheAppSeedPref);
    return uid_provider_->ConvertStringToBinary(pref_seed, expected_len);
  }

  std::unique_ptr<EcheUidProvider> uid_provider_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(EcheUidProviderTest, GetUidHasValue) {
  EXPECT_NE(GetUid(), "");
}

TEST_F(EcheUidProviderTest, GetUidFromCacheShouldBeTheSameOne) {
  std::string uid = GetUid();
  ClearPref(kEcheAppSeedPref);
  EXPECT_EQ(GetUid(), uid);
  ResetUidProvider();
  EXPECT_NE(GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidFromPrefShouldBeTheSameOne) {
  std::string uid = GetUid();
  ResetUidProvider();
  EXPECT_EQ(GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidWithWrongKeyShouldNotBeTheSame) {
  std::string uid = GetUid();
  ResetPrefString(kEcheAppSeedPref, "wrong seed");
  EXPECT_NE(GetUid(), uid);
}

TEST_F(EcheUidProviderTest, BindPendingReceiverCanGetUid) {
  FakeExchangerClient fake_exchanger_client;
  uid_provider_->Bind(fake_exchanger_client.CreatePendingReceiver());

  base::test::TestFuture<const std::string&> future;
  fake_exchanger_client.GetUid(future.GetCallback());

  EXPECT_NE(future.Get(), "");
}

TEST_F(EcheUidProviderTest, GetBinaryWhenSeedSizeCorrect) {
  GetUid();

  EXPECT_NE(DecodeStringWithSeed(kSeedSizeInByte), std::nullopt);
}

TEST_F(EcheUidProviderTest, GetNulloptWhenSeedSizeIncorrect) {
  GetUid();

  EXPECT_EQ(DecodeStringWithSeed(kSeedSizeInByte - 1), std::nullopt);
}

}  // namespace eche_app
}  // namespace ash
