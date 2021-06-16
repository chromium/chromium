// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

constexpr int kNumHash = 11;
constexpr int kNumBitsPerEntry = 16;
const std::string kTestAllowlist[] = {"com.example.test", "my.fake.app",
                                      "yet.another.app"};
double OneDayFromNowMs() {
  return (base::Time::Now() + base::TimeDelta::FromDays(1) -
          base::Time::UnixEpoch())
      .InMillisecondsF();
}

double OneDayAgoMs() {
  return (base::Time::Now() - base::TimeDelta::FromDays(1) -
          base::Time::UnixEpoch())
      .InMillisecondsF();
}

std::unique_ptr<base::DictionaryValue> BuildTestManifest() {
  auto manifest = std::make_unique<base::DictionaryValue>();
  manifest->SetIntPath(kBloomFilterNumHashKey, kNumHash);
  manifest->SetIntPath(kBloomFilterNumBitsKey, 3 * kNumBitsPerEntry);
  manifest->SetDoublePath(kExpiryDateKey, OneDayFromNowMs());

  return manifest;
}

}  // namespace

class AwAppsPackageNamesAllowlistComponentLoaderPolicyTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&allowlist_path_));
  }

  void TearDown() override {
    close(allowlist_fd_);
    base::DeleteFile(allowlist_path_);
  }

  void WriteAllowListToFile(const std::vector<uint8_t>& data) {
    int fd = open(allowlist_path_.MaybeAsASCII().c_str(), O_WRONLY);
    ASSERT_TRUE(base::WriteFileDescriptor(fd, data));
    close(fd);
  }

  void WritePackageNamesAllowListToFile(std::vector<std::string> allowlist) {
    auto filter = std::make_unique<optimization_guide::BloomFilter>(
        kNumHash, kNumBitsPerEntry * allowlist.size());
    for (std::string& package : allowlist) {
      filter->Add(package);
    }
    WriteAllowListToFile(filter->bytes());
  }

  int OpenAndGetAllowlistFd() {
    if (allowlist_fd_ == -1) {
      allowlist_fd_ = open(allowlist_path_.MaybeAsASCII().c_str(), O_RDONLY);
      CHECK(allowlist_fd_) << "Failed to open FD for " << allowlist_path_;
    }
    return allowlist_fd_;
  }

  void LookupConfirmationCallback(bool lookup_result) {
    EXPECT_TRUE(checker_.CalledOnValidSequence());
    lookup_result_ = lookup_result;
    lookup_run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment env_;
  // Has to be init after TaskEnvironment.
  base::SequenceCheckerImpl checker_;
  base::RunLoop lookup_run_loop_;

  bool lookup_result_;

 private:
  int allowlist_fd_ = -1;
  base::FilePath allowlist_path_;
};

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExisitingPackageName) {
  WritePackageNamesAllowListToFile(
      {kTestAllowlist, kTestAllowlist + base::size(kTestAllowlist)});
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_TRUE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestNonExisitingPackageName) {
  WritePackageNamesAllowListToFile(
      {kTestAllowlist, kTestAllowlist + base::size(kTestAllowlist)});
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          "non.existent.app",
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestAllowlistFileNotInMap) {
  base::flat_map<std::string, int> fd_map;
  fd_map["another_file"] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestMissingBloomFilterParams) {
  WritePackageNamesAllowListToFile(
      {kTestAllowlist, kTestAllowlist + base::size(kTestAllowlist)});
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = std::make_unique<base::DictionaryValue>();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooShortBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2, 0xff));
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooLongBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2000, 0xff));
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExpiredAllowlist) {
  WritePackageNamesAllowListToFile(
      {kTestAllowlist, kTestAllowlist + base::size(kTestAllowlist)});
  base::flat_map<std::string, int> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  auto manifest = BuildTestManifest();
  manifest->SetDoublePath(kExpiryDateKey, OneDayAgoMs());

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(lookup_result_);
}

}  // namespace android_webview
