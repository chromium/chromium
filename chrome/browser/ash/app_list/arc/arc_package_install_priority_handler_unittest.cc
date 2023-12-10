// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_package_install_priority_handler.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kPackageName[] = "com.example.app";
}  // namespace

namespace arc {

class ArcPackageInstallPiroirtyHanlderTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(arc::kSyncInstallPriority);
    testing::Test::SetUp();

    arc_app_test_ = std::make_unique<ArcAppTest>();
    arc_app_test_->SetUp(&testing_profile_);
  }

  void TearDown() override {
    arc_app_test_->StopArcInstance();
    arc_app_test_->TearDown();
  }

  ArcAppTest* arc_app_test() { return arc_app_test_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<ArcAppTest> arc_app_test_;
};

TEST_F(ArcPackageInstallPiroirtyHanlderTest, SyncedInstallPrioirty) {
  ArcPackageInstallPriorityHandler* handler =
      arc_app_test()->arc_app_list_prefs()->GetInstallPriorityHandler();
  DCHECK(handler);

  ASSERT_EQ(arc::mojom::InstallPriority::kUndefined,
            handler->GetInstallPriorityForTesting(kPackageName));

  handler->InstallSyncedPacakge(kPackageName,
                                arc::mojom::InstallPriority::kLow);
  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            handler->GetInstallPriorityForTesting(kPackageName));

  handler->PromotePackageInstall(kPackageName);
  ASSERT_EQ(arc::mojom::InstallPriority::kMedium,
            handler->GetInstallPriorityForTesting(kPackageName));

  // Simulated package installation.
  arc::mojom::ArcPackageInfo package;
  package.package_name = kPackageName;
  arc_app_test()->app_instance()->InstallPackage(package.Clone());

  ASSERT_EQ(arc::mojom::InstallPriority::kUndefined,
            handler->GetInstallPriorityForTesting(kPackageName));
}

}  // namespace arc
