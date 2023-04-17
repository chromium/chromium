// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class PromiseAppUpdateTest : public testing::Test {
 public:
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
};

TEST_F(PromiseAppUpdateTest, StateIsNonNull) {
  PromiseApp promise_app = PromiseApp(package_id);
  promise_app.name = "Name";
  promise_app.progress = 0.1;
  promise_app.status = PromiseStatus::kPending;
  promise_app.should_show = true;
  PromiseAppUpdate u(&promise_app, nullptr);

  EXPECT_EQ(package_id, u.PackageId());

  EXPECT_TRUE(u.Name().has_value());
  EXPECT_EQ(u.Name(), "Name");
  EXPECT_EQ(u.NameChanged(), false);

  EXPECT_TRUE(u.Progress().has_value());
  EXPECT_FLOAT_EQ(u.Progress().value(), 0.1);
  EXPECT_EQ(u.ProgressChanged(), false);

  EXPECT_EQ(u.Status(), PromiseStatus::kPending);
  EXPECT_EQ(u.StatusChanged(), false);

  EXPECT_EQ(u.ShouldShow(), true);
  EXPECT_EQ(u.ShouldShowChanged(), false);
}

TEST_F(PromiseAppUpdateTest, DeltaIsNonNull) {
  PromiseApp promise_app = PromiseApp(package_id);
  promise_app.name = "Name";
  promise_app.progress = 0.1;
  promise_app.status = PromiseStatus::kPending;
  promise_app.should_show = true;
  PromiseAppUpdate u(nullptr, &promise_app);

  EXPECT_EQ(package_id, u.PackageId());

  EXPECT_TRUE(u.Name().has_value());
  EXPECT_EQ(u.Name(), "Name");
  EXPECT_EQ(u.NameChanged(), true);

  EXPECT_TRUE(u.Progress().has_value());
  EXPECT_FLOAT_EQ(u.Progress().value(), 0.1);
  EXPECT_EQ(u.ProgressChanged(), true);

  EXPECT_EQ(u.Status(), PromiseStatus::kPending);
  EXPECT_EQ(u.StatusChanged(), true);

  EXPECT_EQ(u.ShouldShow(), true);
  EXPECT_EQ(u.ShouldShowChanged(), true);
}

TEST_F(PromiseAppUpdateTest, StateAndDeltaAreNonNull) {
  PromiseApp promise_app_old = PromiseApp(package_id);
  promise_app_old.name = "Name";
  promise_app_old.progress = 0.1;
  promise_app_old.status = PromiseStatus::kPending;
  promise_app_old.should_show = false;

  PromiseApp promise_app_new = PromiseApp(package_id);
  promise_app_new.name = "New name";
  promise_app_new.progress = 0.9;
  promise_app_new.status = PromiseStatus::kInstalling;
  promise_app_new.should_show = true;

  PromiseAppUpdate u(&promise_app_old, &promise_app_new);

  EXPECT_EQ(package_id, u.PackageId());

  EXPECT_TRUE(u.Name().has_value());
  EXPECT_EQ(u.Name(), "New name");
  EXPECT_EQ(u.NameChanged(), true);

  EXPECT_TRUE(u.Progress().has_value());
  EXPECT_FLOAT_EQ(u.Progress().value(), 0.9);
  EXPECT_EQ(u.ProgressChanged(), true);

  EXPECT_EQ(u.Status(), PromiseStatus::kInstalling);
  EXPECT_EQ(u.StatusChanged(), true);

  EXPECT_EQ(u.ShouldShow(), true);
  EXPECT_EQ(u.ShouldShowChanged(), true);
}

}  // namespace apps
