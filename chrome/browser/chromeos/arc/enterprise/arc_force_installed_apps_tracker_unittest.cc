// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/arc/enterprise/arc_force_installed_apps_tracker.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
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
    arc_app_test_.SetUp(profile_.get());
    tracker_ = ArcForceInstalledAppsTracker::CreateForTesting(prefs(),
                                                              policy_service());
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
    policy_map_.Clear();
    tracker_.reset();
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

  policy::PolicyMap& policy_map() { return policy_map_; }
  policy::MockPolicyService* policy_service() { return &policy_service_; }
  ArcForceInstalledAppsTracker* tracker() { return tracker_.get(); }

 private:
  ArcAppListPrefs* prefs() { return arc_app_test_.arc_app_list_prefs(); }
  arc::mojom::AppHost* const app_host() { return prefs(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  policy::MockPolicyService policy_service_;
  std::unique_ptr<ArcForceInstalledAppsTracker> tracker_;
  policy::PolicyMap policy_map_;
};

TEST_F(ArcForceInstalledAppsTrackerTest, InvalidUpdateCallback) {
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  tracker()->StartTracking(base::NullCallback());

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  tracker()->StopTracking();
}

TEST_F(ArcForceInstalledAppsTrackerTest, EmptyPolicy) {
  EXPECT_CALL(*policy_service(), AddObserver(_, _));
  EXPECT_CALL(*policy_service(), GetPolicies(_))
      .WillOnce(ReturnRef(policy_map()));
  base::RunLoop run_loop;
  tracker()->StartTracking(base::BindLambdaForTesting([&run_loop](int percent) {
    // All tracking apps are installed.
    EXPECT_EQ(100, percent);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  tracker()->StopTracking();
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
      }));

  // Install not required package.
  InstallPackage(kFakePackageName);
  // Install 2 required packages.
  InstallPackage(kBasicPackageName);
  InstallPackage(kBasicPackageName2);

  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  tracker()->StopTracking();
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
      }));

  InstallPackage(kBasicPackageName2);

  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  tracker()->StopTracking();
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
  tracker()->StartTracking(base::BindLambdaForTesting(
      [&closure, &package_number, &max_package_number](int percent) {
        EXPECT_EQ(package_number * 100 / max_package_number, percent);
        package_number++;
        closure.Run();
      }));

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
  tracker()->StopTracking();
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
      }));

  InstallPackage(kBasicPackageName);
  package_number = 0;
  UninstallPackage(kBasicPackageName);
  package_number = 1;
  InstallPackage(kBasicPackageName);
  run_loop.Run();

  EXPECT_CALL(*policy_service(), RemoveObserver(_, _));
  tracker()->StopTracking();
}

}  // namespace data_snapshotd
}  // namespace arc
