// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
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

const char kProfileName[] = "Test";
const char kProfileNameLowerCase[] = "test";
const policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
const char kTestProfilePath[] = "/Path/To/Profile";

const char kProfilePathField[] = "profile_path";
const char kClearCacheField[] = "clear_cache";
const char kClearCookiesField[] = "clear_cookies";

enterprise_management::RemoteCommand CreateCommandProto(
    const std::string& profile_path,
    bool clear_cache,
    bool clear_cookies) {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA);
  command_proto.set_command_id(kUniqueID);

  base::Value::Dict root;
  root.Set(kProfilePathField, profile_path);
  root.Set(kClearCacheField, clear_cache);
  root.Set(kClearCookiesField, clear_cookies);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

void InitJob(ClearBrowsingDataJob* job,
             const enterprise_management::RemoteCommand& command_proto) {
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                        enterprise_management::SignedData{}));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());
}

std::unique_ptr<ClearBrowsingDataJob> CreateJob(
    const enterprise_management::RemoteCommand& command_proto,
    ProfileManager* profile_manager) {
  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager);
  InitJob(job.get(), command_proto);

  return job;
}

std::unique_ptr<ClearBrowsingDataJob> CreateJob(
    const enterprise_management::RemoteCommand& command_proto,
    Profile* profile) {
  auto job = std::make_unique<ClearBrowsingDataJob>(profile);
  InitJob(job.get(), command_proto);

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
    nacl::NaClBrowser::ClearAndDeleteDelegate();
#endif

    ::testing::Test::TearDown();
  }

  ProfileManager* profile_manager() {
    return profile_manager_->profile_manager();
  }

  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

  TestingProfile* AddTestingProfile() {
    return profile_manager_->CreateTestingProfile(kProfileName);
  }

  base::FilePath GetTestProfilePath() {
    return profile_manager_->profiles_dir().AppendASCII(kProfileName);
  }

  base::FilePath GetTestProfilePathLowerCase() {
    return profile_manager_->profiles_dir().AppendASCII(kProfileNameLowerCase);
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

  base::Value::Dict root;
  root.Set(kProfilePathField, kTestProfilePath);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);

  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager());
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                        enterprise_management::SignedData{}));

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

  base::Value::Dict root;
  root.Set(kClearCacheField, true);
  root.Set(kClearCookiesField, true);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  command_proto.set_payload(payload);

  auto job = std::make_unique<ClearBrowsingDataJob>(profile_manager());
  EXPECT_FALSE(job->Init(base::TimeTicks::Now(), command_proto,
                         enterprise_management::SignedData{}));
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
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().AsUTF8Unsafe(),
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
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().AsUTF8Unsafe(),
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
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().AsUTF8Unsafe(),
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
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().AsUTF8Unsafe(),
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

TEST_F(ClearBrowsingDataJobTest, SucessClearWithProfile) {
  base::RunLoop run_loop;
  TestingProfile* profile = AddTestingProfile();
  auto job = CreateJob(CreateCommandProto(GetTestProfilePath().AsUTF8Unsafe(),
                                          /* clear_cache= */ true,
                                          /* clear_cookies= */ false),
                       profile);
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

// For Windows machines, the path that Chrome reports for the profile is
// "Normalized" to all lower-case on the reporting server. This means that
// when the server sends the command, the path will be all lower case and
// the profile manager won't be able to use it as a key.
// Because of that, the code for this job is slightly different on Windows,
// and this test verifies that behavior.
TEST_F(ClearBrowsingDataJobTest, HandleProfilPathCaseSensitivity) {
  base::RunLoop run_loop;

  AddTestingProfile();
  auto job =
      CreateJob(CreateCommandProto(GetTestProfilePathLowerCase().AsUTF8Unsafe(),
                                   /* clear_cache= */ false,
                                   /* clear_cookies= */ true),
                profile_manager());

  bool done = false;

#if BUILDFLAG(IS_WIN)
  // On windows, paths are case-insensitive so passing a lowercase path should
  // still result in success.
  auto expected = policy::RemoteCommandJob::SUCCEEDED;
#else
  // On other platforms, paths are case-sensitive, so passing a lower case path
  // will result in the profile not being found.
  auto expected = policy::RemoteCommandJob::FAILED;
#endif  // BUILDFLAG(IS_WIN)

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       base::BindLambdaForTesting([&] {
                         EXPECT_EQ(expected, job->status());
                         done = true;
                         run_loop.Quit();
                       })));
  run_loop.Run();
  EXPECT_TRUE(done);
}

}  // namespace enterprise_commands
