// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_commands {
namespace {

const char kProfileName[] = "test";
const policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
base::FilePath::StringType kTestProfilePath =
    FILE_PATH_LITERAL("/path/to/profile");

const char kProfilePathField[] = "profile_path";
const char kClearCacheField[] = "clear_cache";
const char kClearCookiesField[] = "clear_cookies";

enterprise_management::RemoteCommand CreateCommandProto(
    const base::FilePath::StringType& profile_path,
    bool clear_cache,
    bool clear_cookies) {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA);
  command_proto.set_command_id(kUniqueID);

  base::Value root(base::Value::Type::DICTIONARY);
  root.SetStringKey(kProfilePathField, profile_path);
  root.SetBoolKey(kClearCacheField, clear_cache);
  root.SetBoolKey(kClearCookiesField, clear_cookies);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

std::unique_ptr<ClearBrowsingDataJob> CreateJob(
    enterprise_management::RemoteCommand command_proto,
    ProfileManager* profile_manager) {
  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager);
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto, nullptr));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

}  // namespace

class ClearBrowsingDataJobTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::testing::Test::SetUp();

    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());

#if BUILDFLAG(ENABLE_NACL)
    // Clearing Cache will clear PNACL cache, which needs this delegate set.
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager()));
#endif
  }

  void TearDown() override {
    profile_manager_.reset();
    task_environment_.reset();

#if BUILDFLAG(ENABLE_NACL)
    // Clearing Cache will clear PNACL cache, which needs this delegate set.
    nacl::NaClBrowser::ClearAndDeleteDelegateForTest();
#endif

    ::testing::Test::TearDown();
  }

  ProfileManager* profile_manager() {
    return profile_manager_->profile_manager();
  }

  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

  void AddTestingProfile() {
    profile_manager_->CreateTestingProfile(kProfileName);
  }

  base::FilePath GetTestProfilePath() {
    return profile_manager_->profiles_dir().AppendASCII(kProfileName);
  }

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ClearBrowsingDataJobTest, CanParseWithMissingDataTypes) {
  base::RunLoop run_loop;

  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA);
  command_proto.set_command_id(kUniqueID);

  base::Value root(base::Value::Type::DICTIONARY);
  root.SetStringKey(kProfilePathField, kTestProfilePath);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);

  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager());
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto, nullptr));

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::FAILED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

TEST_F(ClearBrowsingDataJobTest, DontInitWhenMissingProfilePath) {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA);
  command_proto.set_command_id(kUniqueID);

  base::Value root(base::Value::Type::DICTIONARY);
  root.SetBoolKey(kClearCacheField, true);
  root.SetBoolKey(kClearCookiesField, true);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);

  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager());
  EXPECT_FALSE(job->Init(base::TimeTicks::Now(), command_proto, nullptr));
}

TEST_F(ClearBrowsingDataJobTest, FailureWhenProfileDoesntExist) {
  base::RunLoop run_loop;

  auto job =
      CreateJob(CreateCommandProto(kTestProfilePath, /* clear_cache= */ true,
                                   /* clear_cookies= */ true),
                profile_manager());

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::FAILED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

TEST_F(ClearBrowsingDataJobTest, SuccessClearCookies) {
  base::RunLoop run_loop;

  AddTestingProfile();
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().value(),
                                          /* clear_cache= */ false,
                                          /* clear_cookies= */ true),
                       profile_manager());

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

TEST_F(ClearBrowsingDataJobTest, SuccessClearBoth) {
  base::RunLoop run_loop;

  AddTestingProfile();
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().value(),
                                          /* clear_cache= */ true,
                                          /* clear_cookies= */ false),
                       profile_manager());

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

TEST_F(ClearBrowsingDataJobTest, SuccessClearCache) {
  base::RunLoop run_loop;

  AddTestingProfile();
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().value(),
                                          /* clear_cache= */ true,
                                          /* clear_cookies= */ false),
                       profile_manager());

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

TEST_F(ClearBrowsingDataJobTest, SuccessClearNeither) {
  base::RunLoop run_loop;

  AddTestingProfile();
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().value(),
                                          /* clear_cache= */ false,
                                          /* clear_cookies= */ false),
                       profile_manager());

  bool done = false;
  // Run should return true because the command will be successfully posted,
  // but status of the command will be |FAILED| when |finished_callback| is
  // invoked.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED,
                                   job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

}  // namespace enterprise_commands
