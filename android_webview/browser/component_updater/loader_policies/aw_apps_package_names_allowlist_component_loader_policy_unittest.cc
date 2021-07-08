// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
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
double MillisFromUnixEpoch(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMillisecondsF();
}

std::unique_ptr<base::Value> BuildTestManifest() {
  auto manifest = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  manifest->SetKey(kBloomFilterNumHashKey, base::Value(kNumHash));
  manifest->SetKey(kBloomFilterNumBitsKey, base::Value(3 * kNumBitsPerEntry));
  manifest->SetKey(kExpiryDateKey,
                   base::Value(MillisFromUnixEpoch(
                       base::Time::Now() + base::TimeDelta::FromDays(1))));

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
    base::DeleteFile(allowlist_path_);
  }

  void WriteAllowListToFile(const std::vector<uint8_t>& data) {
    ASSERT_TRUE(base::WriteFile(allowlist_path_, data));
  }

  void WritePackageNamesAllowListToFile() {
    auto filter = std::make_unique<optimization_guide::BloomFilter>(
        kNumHash, kNumBitsPerEntry * base::size(kTestAllowlist));
    for (const auto& package : kTestAllowlist) {
      filter->Add(package);
    }
    WriteAllowListToFile(filter->bytes());
  }

  base::ScopedFD OpenAndGetAllowlistFd() {
    int allowlist_fd = open(allowlist_path_.value().c_str(), O_RDONLY);
    CHECK(allowlist_fd) << "Failed to open FD for " << allowlist_path_;
    return base::ScopedFD(allowlist_fd);
  }

  void LookupConfirmationCallback(absl::optional<base::Time> expiry_date) {
    EXPECT_TRUE(checker_.CalledOnValidSequence());
    allowlist_expiry_date_ = expiry_date;
    lookup_run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment env_;
  // Has to be init after TaskEnvironment.
  base::SequenceCheckerImpl checker_;
  base::RunLoop lookup_run_loop_;

  absl::optional<base::Time> allowlist_expiry_date_;

 private:
  base::FilePath allowlist_path_;
};

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExistingPackageName) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  std::unique_ptr<base::Value> manifest = BuildTestManifest();
  base::Time one_day_from_now =
      base::Time::Now() + base::TimeDelta::FromDays(1);
  manifest->SetDoubleKey(kExpiryDateKey, MillisFromUnixEpoch(one_day_from_now));

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(std::move(manifest)));

  lookup_run_loop_.Run();
  EXPECT_TRUE(allowlist_expiry_date_.has_value());
  EXPECT_EQ(allowlist_expiry_date_.value(), one_day_from_now);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestNonExistingPackageName) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          "non.existent.app",
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(BuildTestManifest()));

  lookup_run_loop_.Run();
  EXPECT_TRUE(allowlist_expiry_date_.has_value());
  EXPECT_TRUE(allowlist_expiry_date_.value().is_min());
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestAllowlistFileNotInMap) {
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map["another_file"] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(BuildTestManifest()));

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_expiry_date_.has_value());
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestMissingBloomFilterParams) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          std::make_unique<base::DictionaryValue>());

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_expiry_date_.has_value());
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooShortBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2, 0xff));
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(BuildTestManifest()));

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_expiry_date_.has_value());
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooLongBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2000, 0xff));
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(BuildTestManifest()));

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_expiry_date_.has_value());
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExpiredAllowlist) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  std::unique_ptr<base::Value> manifest = BuildTestManifest();
  manifest->SetKey(kExpiryDateKey,
                   base::Value(MillisFromUnixEpoch(
                       base::Time::Now() - base::TimeDelta::FromDays(1))));

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(), fd_map,
                          base::DictionaryValue::From(std::move(manifest)));

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_expiry_date_.has_value());
}

}  // namespace android_webview
