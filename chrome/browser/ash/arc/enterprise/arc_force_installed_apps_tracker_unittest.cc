// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_force_installed_apps_tracker.h"

#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ReturnRef;

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kBasicPackageName[] = "basic.package";
constexpr char kBasicPackageName2[] = "basic.package.2";
constexpr char kFakePackageName[] = "fake.package";
constexpr char kArcPolicyValue[] =
    "{\"applications\":"
    "[{\"packageName\":\"basic.package\","
    "\"installType\":\"FORCE_INSTALLED\","
    "}]}";
constexpr char kArcPolicyValue2[] =
    "{\"applications\":"
    "[{\"packageName\":\"basic.package\","
    "\"installType\":\"FORCE_INSTALLED\","
    "},"
    "{\"packageName\":\"basic.package.2\","
    "\"installType\":\"REQUIRED\","
    "}]}";

constexpr char kComplianceReportEmptyJson[] = "{}";
constexpr char kComplianceReportNonCompliantJson[] =
    "{\"nonComplianceDetails\":"
    "[{\"settingName\":\"applications\",\"nonComplianceReason\":1}]}";
constexpr char kComplianceReportCompliantJson[] =
    "{\"nonComplianceDetails\":"
    "[{\"settingName\":\"value\",\"nonComplianceReason\":\"value\"}]}";
constexpr char kComplianceReportAndroidIdNonCompliantJson[] =
    "{\"nonComplianceDetails\":"
    "[{\"settingName\":\"resetAndroidIdEnabled\","
    "\"nonComplianceReason\":1}]}";
}  // namespace

class ArcForceInstalledAppsTrackerTest : public testing::Test {
 public:
  ArcForceInstalledAppsTrackerTest() = default;
  ArcForceInstalledAppsTrackerTest(const ArcForceInstalledAppsTrackerTest&) =
      delete;
  ArcForceInstalledAppsTrackerTest& operator=(
      const ArcForceInstalledAppsTrackerTest&) = delete;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    policy_bridge_ =
        std::make_unique<arc::ArcPolicyBridge>(profile_.get(), &arc_bridge_);
    arc_app_test_.SetUp(profile_.get());
    tracker_ = ArcForceInstalledAppsTracker::CreateForTesting(
        prefs(), policy_service(), policy_bridge_.get());
  }

  void TearDown() override {
    arc_app_test_.TearDown();

    tracker_.reset();
    policy_bridge_.reset();
    profile_.reset();
    policy_map_.Clear();
  }

  void InstallPackage(const std::string& package_name) {
    auto package_info = mojom::ArcPackageInfo::New();
    package_info->package_name = package_name;
    app_host()->OnPackageAdded(std::move(package_info));
  }

  void UninstallPackage(const std::string package_name) {
    app_host()->OnPackageRemoved(package_name);
  }

  void SetArcPolicyValue(const std::string& arc_policy_value) {
    policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                     policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                     base::Value(arc_policy_value), nullptr);
  }

  void ReportCompliant() { ReportCompliance(kComplianceReportCompliantJson); }
  void ReportCompliantEmpty() { ReportCompliance(kComplianceReportEmptyJson); }

  void ReportNonCompliant() {
    ReportCompliance(kComplianceReportNonCompliantJson);
  }

  void ReportAndroidIdNonCompliant() {
    ReportCompliance(kComplianceReportAndroidIdNonCompliantJson);
  }

  policy::PolicyMap& policy_map() { return policy_map_; }
  policy::MockPolicyService* policy_service() { return &policy_service_; }
  arc::ArcPolicyBridge* arc_policy_bridge() { return policy_bridge_.get(); }
  ArcForceInstalledAppsTracker* tracker() { return tracker_.get(); }

 private:
  void ReportCompliance(const std::string& compliance_json) {
    base::RunLoop run_loop;
    arc_policy_bridge()->ReportCompliance(
        compliance_json,
        base::BindLambdaForTesting(
            [&run_loop](const std::string& response) { run_loop.Quit(); }));
    run_loop.Run();
  }

  ArcAppListPrefs* prefs() { return arc_app_test_.arc_app_list_prefs(); }
  arc::mojom::AppHost* app_host() { return prefs(); }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  policy::MockPolicyService policy_service_;
  arc::ArcBridgeService arc_bridge_;
  std::unique_ptr<arc::ArcPolicyBridge> policy_bridge_;
  std::unique_ptr<ArcForceInstalledAppsTracker> tracker_;
  policy::PolicyMap policy_map_;
};

TEST_F(ArcForceInstalledAppsTrackerTest, InvalidCallbacks) {
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  tracker()->StartTracking(base::NullCallback(), base::NullCallback());

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
}

TEST_F(ArcForceInstalledAppsTrackerTest, EmptyPolicy) {
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  bool is_policy_compliant = false;
  base::RunLoop run_loop;
  tracker()->StartTracking(base::BindLambdaForTesting([&run_loop](int percent) {
                             // All tracking apps are installed.
                             EXPECT_EQ(100, percent);
                             run_loop.Quit();
                           }),
                           base::BindLambdaForTesting([&is_policy_compliant]() {
                             is_policy_compliant = true;
                           }));
  run_loop.Run();

  EXPECT_FALSE(is_policy_compliant);

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  ReportCompliantEmpty();
  EXPECT_TRUE(is_policy_compliant);
}

TEST_F(ArcForceInstalledAppsTrackerTest, Basic) {
  SetArcPolicyValue(kArcPolicyValue2);
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(3, run_loop.QuitClosure());
  int package_number = 0;
  tracker()->StartTracking(
      base::BindLambdaForTesting([&closure, &package_number](int percent) {
        EXPECT_EQ(package_number * 50, percent);
        package_number++;
        closure.Run();
      }),
      base::DoNothing());

  // Install not required package.
  InstallPackage(kFakePackageName);
  // Install 2 required packages.
  InstallPackage(kBasicPackageName);
  InstallPackage(kBasicPackageName2);

  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
}

TEST_F(ArcForceInstalledAppsTrackerTest, AlreadyInstalledPackages) {
  InstallPackage(kBasicPackageName);

  SetArcPolicyValue(kArcPolicyValue2);
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));

  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  int package_number = 1;
  tracker()->StartTracking(
      base::BindLambdaForTesting([&closure, &package_number](int percent) {
        EXPECT_EQ(package_number * 50, percent);
        package_number++;
        closure.Run();
      }),
      base::DoNothing());

  InstallPackage(kBasicPackageName2);

  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
}

// Tests the case when tracking packages list changes.
TEST_F(ArcForceInstalledAppsTrackerTest, OnPolicyUpdated) {
  SetArcPolicyValue(kArcPolicyValue2);
  policy::PolicyService::Observer* observer = nullptr;
  EXPECT_CALL(*policy_service(), AddObserver(_, _))
      .WillOnce(testing::SaveArg<1>(&observer));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map()));
  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(4, run_loop.QuitClosure());
  int package_number = 0;
  int max_package_number = 2;
  tracker()->StartTracking(
      base::BindLambdaForTesting(
          [&closure, &package_number, &max_package_number](int percent) {
            EXPECT_EQ(package_number * 100 / max_package_number, percent);
            package_number++;
            closure.Run();
          }),
      base::DoNothing());

  InstallPackage(kBasicPackageName2);

  SetArcPolicyValue(kArcPolicyValue);
  package_number = 0;
  max_package_number = 1;
  EXPECT_TRUE(observer);

  const policy::PolicyNamespace policy_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  observer->OnPolicyUpdated(policy_namespace, policy::PolicyMap(),
                            policy_service()->GetPolicies(policy_namespace));

  InstallPackage(kBasicPackageName);

  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
}

// Tests the uninstall tracking package scenario.
TEST_F(ArcForceInstalledAppsTrackerTest, UninstalledPackages) {
  SetArcPolicyValue(kArcPolicyValue);
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  base::RunLoop run_loop;
  base::RepeatingClosure closure =
      base::BarrierClosure(4, run_loop.QuitClosure());
  int package_number = 0;
  tracker()->StartTracking(
      base::BindLambdaForTesting([&closure, &package_number](int percent) {
        EXPECT_EQ(package_number * 100, percent);
        package_number++;
        closure.Run();
      }),
      base::DoNothing());

  InstallPackage(kBasicPackageName);
  package_number = 0;
  UninstallPackage(kBasicPackageName);
  package_number = 1;
  InstallPackage(kBasicPackageName);
  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
}

TEST_F(ArcForceInstalledAppsTrackerTest, PolicyCompliance) {
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  bool is_policy_compliant = false;
  tracker()->StartTracking(base::DoNothing(),
                           base::BindLambdaForTesting([&is_policy_compliant]() {
                             is_policy_compliant = true;
                           }));

  EXPECT_FALSE(is_policy_compliant);

  ReportNonCompliant();
  EXPECT_FALSE(is_policy_compliant);

  ReportAndroidIdNonCompliant();
  EXPECT_FALSE(is_policy_compliant);

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  ReportCompliant();
  EXPECT_TRUE(is_policy_compliant);

  // Report compliance second time. Ignore.
  is_policy_compliant = false;
  ReportCompliant();
  EXPECT_FALSE(is_policy_compliant);
}

TEST_F(ArcForceInstalledAppsTrackerTest, PolicyCompliantOnStart) {
  ReportCompliant();

  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  base::RunLoop run_loop;
  tracker()->StartTracking(
      base::DoNothing(),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  run_loop.Run();
}

}  // namespace data_snapshotd
}  // namespace arc
