// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_policy_util.h"

#include <map>
#include <string>

#include "arc_policy_util.h"
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
constexpr char kArcPolicyKeyHistogram[] = "Arc.Policy.Keys";

std::map<std::string, std::string> kTestMap = {
    {"testPackage", "FORCE_INSTALLED"}, {"testPackage2", "BLOCKED"},
    {"testPackage3", "BLOCKED"},        {"testPackage4", "AVAILABLE"},
    {"testPackage5", "AVAILABLE"},      {"testPackage6", "REQUIRED"}};

std::string CreatePolicyJson(const base::Value::Dict& arc_policy) {
  std::string arc_policy_string;
  base::JSONWriter::Write(arc_policy, &arc_policy_string);
  return arc_policy_string;
}

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
  return CreatePolicyJson(arc_policy);
}

std::string CreatePolicyWithKeys(std::set<std::string> keys) {
  base::Value::Dict arc_policy;

  for (const std::string& key : keys) {
    arc_policy.Set(key, "value");
  }

  return CreatePolicyJson(arc_policy);
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

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsWithIgnoredKeys) {
  std::set<std::string> test_keys = {
      kArcPolicyKeyGuid,
      kArcPolicyKeyAvailableAppSetPolicyDeprecated,
      kArcPolicyKeyWorkAccountAppWhitelistDeprecated,
      kArcPolicyKeyMountPhysicalMediaDisabled,
      kArcPolicyKeyDpsInteractionsDisabled,
  };
  std::string policy = CreatePolicyWithKeys(test_keys);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectTotalCount(kArcPolicyKeyHistogram, 0);
}

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsWithUnknownKeys) {
  std::set<std::string> test_keys = {
      "some_unknown_key",
  };
  std::string policy = CreatePolicyWithKeys(test_keys);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectBucketCount(kArcPolicyKeyHistogram, ArcPolicyKey::kUnknown, 1);
  tester_.ExpectTotalCount(kArcPolicyKeyHistogram, 1);
}

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsWithKnownKeys) {
  std::set<std::string> test_keys = {
      kArcPolicyKeyAccountTypesWithManagementDisabled,
      kArcPolicyKeyAlwaysOnVpnPackage,
      kArcPolicyKeyApplications,
      kArcPolicyKeyComplianceRules,
      kArcPolicyKeyInstallUnknownSourcesDisabled,
      kArcPolicyKeyMaintenanceWindow,
      kArcPolicyKeyModifyAccountsDisabled,
      kArcPolicyKeyPermissionGrants,
      kArcPolicyKeyPermittedAccessibilityServices,
      kArcPolicyKeyPlayStoreMode,
      kArcPolicyKeyShortSupportMessage,
      kArcPolicyKeyStatusReportingSettings,
      kArcPolicyKeyApkCacheEnabled,
      kArcPolicyKeyDebuggingFeaturesDisabled,
      kArcPolicyKeyCameraDisabled,
      kArcPolicyKeyPrintingDisabled,
      kArcPolicyKeyScreenCaptureDisabled,
      kArcPolicyKeyShareLocationDisabled,
      kArcPolicyKeyUnmuteMicrophoneDisabled,
      kArcPolicyKeySetWallpaperDisabled,
      kArcPolicyKeyVpnConfigDisabled,
      kArcPolicyKeyPrivateKeySelectionEnabled,
      kArcPolicyKeyChoosePrivateKeyRules,
      kArcPolicyKeyCredentialsConfigDisabled,
      kArcPolicyKeyCaCerts,
      kArcPolicyKeyRequiredKeyPairs,
      kArcPolicyKeyEnabledSystemAppPackageNames,
  };
  std::string policy = CreatePolicyWithKeys(test_keys);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectBucketCount(kArcPolicyKeyHistogram, ArcPolicyKey::kUnknown, 0);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kAccountTypesWithManagementDisabled,
                            1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kAlwaysOnVpnPackage, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram, ArcPolicyKey::kApplications,
                            1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kComplianceRules, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kInstallUnknownSourcesDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kMaintenanceWindow, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kModifyAccountsDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kPermissionGrants, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kPermittedAccessibilityServices, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kPlayStoreMode, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kShortSupportMessage, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kStatusReportingSettings, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kApkCacheEnabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kDebuggingFeaturesDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kCameraDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kPrintingDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kScreenCaptureDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kShareLocationDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kUnmuteMicrophoneDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kSetWallpaperDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kVpnConfigDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kPrivateKeySelectionEnabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kChoosePrivateKeyRules, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kCredentialsConfigDisabled, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram, ArcPolicyKey::kCaCerts, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kRequiredKeyPairs, 1);
  tester_.ExpectBucketCount(kArcPolicyKeyHistogram,
                            ArcPolicyKey::kEnabledSystemAppPackageNames, 1);
  tester_.ExpectTotalCount(kArcPolicyKeyHistogram, 27);
}

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsWithOneAppOfEachType) {
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

  arc::policy_util::RecordPolicyMetrics(policy);

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

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsWithComplexPolicy) {
  std::string policy = CreatePolicyWithAppInstalls(kTestMap);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kBlockedBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kAvailableBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kRequiredBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 4);
}

TEST_F(ArcPolicyUtilTest, RecordPolicyMetricsAfterPolicyUpdate) {
  std::map<std::string, std::string> test_map = {
      {"testPackage", "FORCE_INSTALLED"}};
  std::string policy = CreatePolicyWithAppInstalls(test_map);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 1);

  test_map["anotherTestPackage"] = "BLOCKED";
  test_map["anotherTestPackage2"] = "KIOSK";
  policy = CreatePolicyWithAppInstalls(test_map);

  arc::policy_util::RecordPolicyMetrics(policy);

  tester_.ExpectBucketCount(kInstallTypeHistogram, kForceInstalledBucket, 2);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kBlockedBucket, 1);
  tester_.ExpectBucketCount(kInstallTypeHistogram, kKioskBucket, 1);
  tester_.ExpectTotalCount(kInstallTypeHistogram, 4);
}

}  // namespace arc::policy_util
