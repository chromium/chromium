// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/android_app_info_generator.h"

#include <memory>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace am = arc::mojom;
namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {
constexpr char kAppName[] = "app_name";
constexpr char kPackageName[] = "package_name";
constexpr char kActivityName[] = "activity_name";
constexpr int kPackageVersion = 9;

am::AppInfoPtr CreateArcApp(bool suspended) {
  auto app = am::AppInfo::New();
  app->name = kAppName;
  app->package_name = kPackageName;
  app->activity = kActivityName;
  app->suspended = suspended;
  app->sticky = true;
  app->notifications_enabled = true;
  return app;
}

am::ArcPackageInfoPtr CreateArcPackage(const std::string& package_name,
                                       int package_version) {
  base::flat_map<am::AppPermission, am::PermissionStatePtr> permissions;

  permissions.emplace(
      am::AppPermission::CAMERA,
      am::PermissionState::New(false /* granted */, false /* managed */));
  permissions.emplace(
      am::AppPermission::LOCATION,
      am::PermissionState::New(true /* granted */, true /* managed */));

  return am::ArcPackageInfo::New(
      package_name, package_version, 1 /* last_backup_android_id */,
      1 /* last_backup_time */, true /* sync */, false /* system */,
      false /* vpn_provider */, nullptr /* web_app_info */, std::nullopt,
      std::move(permissions) /* permission states */);
}

}  // namespace

class AndroidAppInfoGeneratorTest : public ::testing::Test {
 public:
  AndroidAppInfoGeneratorTest() = default;
  AndroidAppInfoGeneratorTest(const AndroidAppInfoGeneratorTest&) = delete;
  AndroidAppInfoGeneratorTest& operator=(const AndroidAppInfoGeneratorTest&) =
      delete;
  ~AndroidAppInfoGeneratorTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    arc_app_test_.SetUp(&profile_);
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    testing::Test::TearDown();
  }

  void AddArcApp(am::AppInfoPtr arc_app_ptr) {
    auto apps = std::vector<am::AppInfoPtr>();
    apps.emplace_back(std::move(arc_app_ptr));
    arc_app_test()->app_instance()->SendRefreshAppList(apps);
  }

  void AddArcPackage(am::ArcPackageInfoPtr arc_package_ptr) {
    arc_app_test()->app_instance()->SendPackageAdded(
        std::move(arc_package_ptr));
  }

  AndroidAppInfoGenerator* app_info_generator() { return &app_info_generator_; }
  ArcAppTest* arc_app_test() { return &arc_app_test_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  AndroidAppInfoGenerator app_info_generator_;
  ArcAppTest arc_app_test_;
};

TEST_F(AndroidAppInfoGeneratorTest, GenerateAppInfo) {
  ArcAppListPrefs* prefs = arc_app_test()->arc_app_list_prefs();
  AddArcPackage(CreateArcPackage(kPackageName, kPackageVersion));
  AddArcApp(CreateArcApp(false /* suspended */));

  std::string app_id = prefs->GetAppId(kPackageName, kActivityName);
  std::unique_ptr<em::AndroidAppInfo> app_info =
      app_info_generator()->Generate(prefs, app_id);

  EXPECT_EQ(app_info->app_id(), app_id);
  EXPECT_EQ(app_info->app_name(), kAppName);
  EXPECT_EQ(app_info->package_name(), kPackageName);
  EXPECT_EQ(app_info->status(), em::AndroidAppInfo::STATUS_ENABLED);
  EXPECT_EQ(app_info->installed_source(), em::AndroidAppInfo::SOURCE_BY_USER);
  EXPECT_EQ(app_info->version(), kPackageVersion);
  EXPECT_EQ(app_info->permissions_size(), 2);

  em::AndroidAppPermission permission0 = app_info->permissions(0);
  EXPECT_EQ(permission0.name(), "CAMERA");
  EXPECT_FALSE(permission0.granted());
  EXPECT_FALSE(permission0.managed());

  em::AndroidAppPermission permission1 = app_info->permissions(1);
  EXPECT_EQ(permission1.name(), "LOCATION");
  EXPECT_TRUE(permission1.granted());
  EXPECT_TRUE(permission1.managed());
}

TEST_F(AndroidAppInfoGeneratorTest, GenerateAppInfoWithSuspendedStatus) {
  ArcAppListPrefs* prefs = arc_app_test()->arc_app_list_prefs();
  AddArcPackage(CreateArcPackage(kPackageName, kPackageVersion));
  AddArcApp(CreateArcApp(true /* suspended */));

  std::string app_id = prefs->GetAppId(kPackageName, kActivityName);
  std::unique_ptr<em::AndroidAppInfo> app_info =
      app_info_generator()->Generate(prefs, app_id);

  EXPECT_EQ(app_info->status(), em::AndroidAppInfo::STATUS_SUSPENDED);
}

TEST_F(AndroidAppInfoGeneratorTest, GenerateAppInfoInstalledByAdmin) {
  ArcAppListPrefs* prefs = arc_app_test()->arc_app_list_prefs();
  AddArcPackage(CreateArcPackage(kPackageName, kPackageVersion));
  AddArcApp(CreateArcApp(false /* suspended */));

  // Set policy to install the package enforcedly.
  const std::string policy = base::StringPrintf(
      "{\"applications\":[{\"installType\":\"FORCE_INSTALLED\","
      "\"packageName\":\"%s\"}]}",
      kPackageName);
  prefs->OnPolicySent(policy);

  std::string app_id = prefs->GetAppId(kPackageName, kActivityName);
  std::unique_ptr<em::AndroidAppInfo> app_info =
      app_info_generator()->Generate(prefs, app_id);

  EXPECT_EQ(app_info->installed_source(), em::AndroidAppInfo::SOURCE_BY_ADMIN);
}

TEST_F(AndroidAppInfoGeneratorTest, GenerateAppInfoWithoutPackage) {
  ArcAppListPrefs* prefs = arc_app_test()->arc_app_list_prefs();
  AddArcApp(CreateArcApp(false /* suspended */));

  std::string app_id = prefs->GetAppId(kPackageName, kActivityName);
  std::unique_ptr<em::AndroidAppInfo> app_info =
      app_info_generator()->Generate(prefs, app_id);

  EXPECT_EQ(app_info->app_id(), app_id);
  EXPECT_EQ(app_info->app_name(), kAppName);
  EXPECT_EQ(app_info->package_name(), kPackageName);
  EXPECT_EQ(app_info->status(), em::AndroidAppInfo::STATUS_ENABLED);
  EXPECT_EQ(app_info->installed_source(), em::AndroidAppInfo::SOURCE_BY_USER);

  // Following fields are empty because package is not found.
  EXPECT_FALSE(app_info->has_version());
  EXPECT_EQ(app_info->permissions_size(), 0);
}

TEST_F(AndroidAppInfoGeneratorTest, GenerateAppInfoWithInvalidAppId) {
  ArcAppListPrefs* prefs = arc_app_test()->arc_app_list_prefs();
  std::string app_id = "invalid_app_id";
  std::unique_ptr<em::AndroidAppInfo> app_info =
      app_info_generator()->Generate(prefs, app_id);

  // Following fields are empty because application and corresponding package
  // are not found.
  EXPECT_FALSE(app_info->has_app_id());
  EXPECT_FALSE(app_info->has_app_name());
  EXPECT_FALSE(app_info->has_package_name());
  EXPECT_FALSE(app_info->has_status());
  EXPECT_FALSE(app_info->has_installed_source());
  EXPECT_FALSE(app_info->has_version());
  EXPECT_EQ(app_info->permissions_size(), 0);
}

}  // namespace enterprise_reporting
