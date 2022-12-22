// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_util.h"

#include <map>
#include <string>

#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc::policy_util {

namespace {

constexpr int kUnknownBucket = 0;
constexpr int kOptionalBucket = 1;
constexpr int kRequiredBucket = 2;
constexpr int kPreloadBucket = 3;
constexpr int kForceInstalledBucket = 4;
constexpr int kBlockedBucket = 5;
constexpr int kAvailableBucket = 6;
constexpr int kRequiredForSetupBucket = 7;
constexpr int kKioskBucket = 8;
constexpr char kInstallTypeHistogram[] = "Arc.Policy.InstallTypesOnDevice";

std::map<std::string, std::string> kTestMap = {
    {"testPackage", "FORCE_INSTALLED"}, {"testPackage2", "BLOCKED"},
    {"testPackage3", "BLOCKED"},        {"testPackage4", "AVAILABLE"},
    {"testPackage5", "AVAILABLE"},      {"testPackage6", "REQUIRED"}};

std::string CreatePolicyWithAppInstalls(
    std::map<std::string, std::string> package_map) {
  base::Value::Dict arc_policy;
  base::Value::List list;

  for (const auto& entry : package_map) {
    base::Value::Dict package;
    package.Set("packageName", entry.first);
    package.Set("installType", entry.second);
    list.Append(std::move(package));
  }

  arc_policy.Set("applications", std::move(list));
  std::string arc_policy_string;
  base::JSONWriter::Write(arc_policy, &arc_policy_string);
  return arc_policy_string;
}

}  // namespace

class ArcPolicyUtilTest : public testing::Test {
 public:
  ArcPolicyUtilTest(const ArcPolicyUtilTest&) = delete;
  ArcPolicyUtilTest& operator=(const ArcPolicyUtilTest&) = delete;

 protected:
  ArcPolicyUtilTest() = default;

  base::HistogramTester tester_;
};

TEST_F(ArcPolicyUtilTest, GetRequestedPackagesFromArcPolicy) {
  std::set<std::string> expected = {"testPackage", "testPackage6"};
  std::string policy = CreatePolicyWithAppInstalls(kTestMap);
  std::set<std::string> result =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);

  EXPECT_EQ(result, expected);
}

TEST_F(ArcPolicyUtilTest, RecordInstallTypesInPolicyWithOneOfEachType) {
  std::map<std::string, std::string> test_map = {
      {"testPackage", "OPTIONAL"},
      {"testPackage2", "REQUIRED"},
      {"testPackage3", "PRELOAD"},
      {"testPackage4", "FORCE_INSTALLED"},
      {"testPackage5", "BLOCKED"},
      {"testPackage6", "AVAILABLE"},
      {"testPackage7", "REQUIRED_FOR_SETUP"},
      {"testPackage8", "KIOSK"},
      {"testPackage9", "UNKNOWN"}};

  std::string policy = CreatePolicyWithAppInstalls(test_map);
  arc::policy_util::RecordInstallTypesInPolicy(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kUnknownBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kOptionalBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kRequiredBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kPreloadBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kBlockedBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kAvailableBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kRequiredForSetupBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kKioskBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 9);
}

TEST_F(ArcPolicyUtilTest, RecordInstallTypesInPolicyWithComplexPolicy) {
  std::string policy = CreatePolicyWithAppInstalls(kTestMap);
  arc::policy_util::RecordInstallTypesInPolicy(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kBlockedBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kAvailableBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kRequiredBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 4);
}

TEST_F(ArcPolicyUtilTest, RecordInstallTypesInPolicyAfterPolicyUpdate) {
  std::map<std::string, std::string> test_map = {
      {"testPackage", "FORCE_INSTALLED"}};
  std::string policy = CreatePolicyWithAppInstalls(test_map);
  arc::policy_util::RecordInstallTypesInPolicy(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 1);

  test_map["anotherTestPackage"] = "BLOCKED";
  test_map["anotherTestPackage2"] = "KIOSK";
  policy = CreatePolicyWithAppInstalls(test_map);
  arc::policy_util::RecordInstallTypesInPolicy(policy);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 2);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kBlockedBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kKioskBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 4);
}

}  // namespace arc::policy_util
