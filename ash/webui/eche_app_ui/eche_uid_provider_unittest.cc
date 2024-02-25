// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_uid_provider.h"

#include "base/base64.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
};

class EcheUidProviderTest;

class Callback {
 public:
  static void GetUidCallback(const std::string& uid) {
    uid_ = uid;
    if (task_runner_) {
      task_runner_->Finish();
    }
  }

  static void setTaskRunner(TaskRunner* task_runner) {
    task_runner_ = task_runner;
  }

  static std::string GetUid() { return uid_; }
  static void ResetUid() { uid_ = ""; }
  static void ResetTaskRunner() { task_runner_ = nullptr; }

 private:
  static TaskRunner* task_runner_;
  static std::string uid_;
};

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

ash::eche_app::TaskRunner* ash::eche_app::Callback::task_runner_ = nullptr;
std::string ash::eche_app::Callback::uid_ = "";

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
    Callback::ResetUid();
    Callback::ResetTaskRunner();
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
  void GetUid() {
    uid_provider_->GetUid(base::BindOnce(&Callback::GetUidCallback));
  }
  std::optional<std::vector<uint8_t>> DecodeStringWithSeed(
      size_t expected_len) {
    std::string pref_seed = pref_service_.GetString(kEcheAppSeedPref);
    return uid_provider_->ConvertStringToBinary(pref_seed, expected_len);
  }

  TaskRunner task_runner_;
  std::unique_ptr<EcheUidProvider> uid_provider_;

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(EcheUidProviderTest, GetUidHasValue) {
  GetUid();
  EXPECT_NE(Callback::GetUid(), "");
}

TEST_F(EcheUidProviderTest, GetUidFromCacheShouldBeTheSameOne) {
  GetUid();
  std::string uid = Callback::GetUid();
  GetUid();
  EXPECT_EQ(Callback::GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidFromPrefShouldBeTheSameOne) {
  GetUid();
  std::string uid = Callback::GetUid();
  ResetUidProvider();
  GetUid();
  EXPECT_EQ(Callback::GetUid(), uid);
}

TEST_F(EcheUidProviderTest, GetUidWithWrongKeyShouldNotBeTheSame) {
  GetUid();
  std::string uid = Callback::GetUid();
  ResetPrefString(kEcheAppSeedPref, "wrong seed");
  GetUid();
  EXPECT_NE(Callback::GetUid(), uid);
}

TEST_F(EcheUidProviderTest, BindPendingReceiverCanGetUid) {
  Callback::setTaskRunner(&task_runner_);
  FakeExchangerClient fake_exchanger_client;
  uid_provider_->Bind(fake_exchanger_client.CreatePendingReceiver());

  fake_exchanger_client.GetUid(base::BindOnce(&Callback::GetUidCallback));
  task_runner_.WaitForResult();

  EXPECT_NE(Callback::GetUid(), "");
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
