// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"

#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class PromiseAppUpdateTest : public testing::Test {
 public:
  PackageId package_id = PackageId(PackageType::kArc, "test.package.name");
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

  EXPECT_EQ(u.InstalledAppId(), "");
  EXPECT_FALSE(u.InstalledAppIdChanged());
}

TEST_F(PromiseAppUpdateTest, DeltaIsNonNull) {
  PromiseApp promise_app = PromiseApp(package_id);
  promise_app.name = "Name";
  promise_app.progress = 0.1;
  promise_app.status = PromiseStatus::kPending;
  promise_app.should_show = true;
  promise_app.installed_app_id = "app1";
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

  EXPECT_EQ(u.InstalledAppId(), "app1");
  EXPECT_TRUE(u.InstalledAppIdChanged());
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
  promise_app_new.installed_app_id = "app1";

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

  EXPECT_EQ(u.InstalledAppId(), "app1");
  EXPECT_TRUE(u.InstalledAppIdChanged());
}

TEST_F(PromiseAppUpdateTest, Equal) {
  auto state_1 = std::make_unique<PromiseApp>(package_id);
  state_1->status = PromiseStatus::kPending;
  state_1->should_show = false;

  auto state_2 = std::make_unique<PromiseApp>(package_id);
  state_2->name = "Name";
  state_2->progress = 0.9;
  state_2->status = PromiseStatus::kInstalling;
  state_2->should_show = true;

  auto delta_1 = std::make_unique<PromiseApp>(package_id);
  delta_1->status = PromiseStatus::kInstalling;
  delta_1->should_show = true;
  delta_1->installed_app_id = "app1";

  auto delta_2 = std::make_unique<PromiseApp>(package_id);
  delta_2->progress = 0.9;
  delta_2->status = PromiseStatus::kInstalling;

  // Test nullptr handling.
  EXPECT_EQ(PromiseAppUpdate(nullptr, delta_1.get()),
            PromiseAppUpdate(nullptr, delta_1.get()));
  EXPECT_EQ(PromiseAppUpdate(state_1.get(), nullptr),
            PromiseAppUpdate(state_1.get(), nullptr));
  EXPECT_NE(PromiseAppUpdate(nullptr, delta_1.get()),
            PromiseAppUpdate(state_1.get(), nullptr));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), nullptr),
            PromiseAppUpdate(nullptr, delta_1.get()));
  EXPECT_NE(PromiseAppUpdate(nullptr, delta_1.get()),
            PromiseAppUpdate(state_1.get(), delta_1.get()));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), nullptr),
            PromiseAppUpdate(state_1.get(), delta_1.get()));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(nullptr, delta_1.get()));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_1.get(), nullptr));

  // Test equal.
  EXPECT_EQ(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_1.get(), delta_1.get()));
  EXPECT_EQ(PromiseAppUpdate(state_1.get(), delta_2.get()),
            PromiseAppUpdate(state_1.get(), delta_2.get()));
  EXPECT_EQ(PromiseAppUpdate(state_2.get(), delta_1.get()),
            PromiseAppUpdate(state_2.get(), delta_1.get()));
  EXPECT_EQ(PromiseAppUpdate(state_2.get(), delta_2.get()),
            PromiseAppUpdate(state_2.get(), delta_2.get()));

  // Test deep equal.
  EXPECT_EQ(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_1->Clone().get(), delta_1->Clone().get()));

  // Test not equal.
  EXPECT_NE(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_2.get(), delta_1.get()));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_2.get(), delta_2.get()));
  EXPECT_NE(PromiseAppUpdate(state_1.get(), delta_1.get()),
            PromiseAppUpdate(state_1.get(), delta_2.get()));
}

}  // namespace apps
