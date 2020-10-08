// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // OS_CHROMEOS

namespace reporting {
namespace {

using policy::MockCloudPolicyClient;
using testing::_;
using testing::Invoke;
using testing::InvokeArgument;
using testing::WithArgs;

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {}

  virtual void Signal() { run_loop_->Quit(); }

  void Wait() { run_loop_->Run(); }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class TestCallbackWaiterWithCounter : public TestCallbackWaiter {
 public:
  explicit TestCallbackWaiterWithCounter(size_t counter_limit)
      : counter_limit_(counter_limit) {
    DCHECK_GT(counter_limit, 0u);
  }

  void Signal() override {
    const size_t old_count = counter_limit_.fetch_sub(1);
    DCHECK_GT(old_count, 0u);
    if (old_count > 1) {
      return;
    }
    run_loop_->Quit();
  }

 private:
  std::atomic<size_t> counter_limit_;
};

class UploadClientTest : public ::testing::Test {
 public:
  UploadClientTest() = default;

 protected:
  void SetUp() override {
#ifdef OS_CHROMEOS
    // Set up fake primary profile.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<chromeos::FakeChromeUserManager>>();
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef")));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
    const user_manager::User* user =
        mock_user_manager->AddPublicAccountUser(account_id);
    mock_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
#endif  // OS_CHROMEOS
  }

  void TearDown() override {
#ifdef OS_CHROMEOS
    user_manager_.reset();
    profile_.reset();
#endif  // OS_CHROMEOS
  }

  content::BrowserTaskEnvironment task_envrionment_;
#ifdef OS_CHROMEOS
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // OS_CHROMEOS
};

TEST_F(UploadClientTest, CreateUploadClient) {
  const int kExpectedCallTimes = 10;
  const uint64_t kGenerationId = 1234;

  TestCallbackWaiterWithCounter waiter(kExpectedCallTimes);

  auto client = std::make_unique<MockCloudPolicyClient>();
  client->SetDMToken(
      policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());

  EXPECT_CALL(*client, UploadExtensionInstallReport_(_, _))
      .WillRepeatedly(WithArgs<1>(
          Invoke([&waiter](AppInstallReportHandler::ClientCallback& callback) {
            std::move(callback).Run(true);
            base::ThreadPool::PostTask(
                FROM_HERE, {base::TaskPriority::BEST_EFFORT},
                base::BindOnce(&TestCallbackWaiterWithCounter::Signal,
                               base::Unretained(&waiter)));
          })));

  TestEvent<StatusOr<std::unique_ptr<UploadClient>>> e;
  UploadClient::Create(std::move(client), base::DoNothing(), e.cb());
  StatusOr<std::unique_ptr<UploadClient>> upload_client_result = e.result();
  ASSERT_OK(upload_client_result) << upload_client_result.status();

  base::Value data{base::Value::Type::DICTIONARY};
  data.SetKey("TEST_KEY", base::Value("TEST_VALUE"));

  std::string json_data;
  ASSERT_TRUE(base::JSONWriter::Write(data, &json_data));

  WrappedRecord wrapped_record;
  Record* record = wrapped_record.mutable_record();
  record->set_data(json_data);
  record->set_destination(Destination::APP_INSTALL_EVENT);

  std::string serialized_record;
  wrapped_record.SerializeToString(&serialized_record);

  std::unique_ptr<std::vector<EncryptedRecord>> records =
      std::make_unique<std::vector<EncryptedRecord>>();
  for (int i = 0; i < kExpectedCallTimes; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(serialized_record);

    SequencingInformation* sequencing_information =
        encrypted_record.mutable_sequencing_information();
    sequencing_information->set_sequencing_id(i);
    sequencing_information->set_generation_id(kGenerationId);
    sequencing_information->set_priority(Priority::IMMEDIATE);
    records->push_back(encrypted_record);
  }

  auto upload_client = std::move(upload_client_result.ValueOrDie());
  auto enqueue_result = upload_client->EnqueueUpload(std::move(records));
  EXPECT_TRUE(enqueue_result.ok());

  waiter.Wait();
}

}  // namespace
}  // namespace reporting
