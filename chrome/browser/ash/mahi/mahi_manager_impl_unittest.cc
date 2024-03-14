// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_helper.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

using ::testing::IsNull;

using crosapi::mojom::MahiContextMenuActionType;

class FakeMahiProvider : public manta::MahiProvider {
 public:
  FakeMahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : MahiProvider(std::move(test_url_loader_factory), identity_manager) {}

  void Summarize(const std::string& input,
                 manta::MantaGenericCallback callback) {
    std::move(callback).Run(base::Value::Dict(),
                            {manta::MantaStatusCode::kOk, "Status string ok"});
  }
};

}  // namespace

namespace ash {

class MahiManagerImplTest : public testing::Test {
 public:
  MahiManagerImplTest() = default;

  MahiManagerImplTest(const MahiManagerImplTest&) = delete;
  MahiManagerImplTest& operator=(const MahiManagerImplTest&) = delete;

  ~MahiManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp();
    crosapi_environment_.SetUp();

    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    mahi_manager_impl_->mahi_provider_ = CreateMahiProvider();
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    crosapi_environment_.TearDown();
    ash_test_helper_.TearDown();
  }

  views::Widget* GetMahiPanelWidget() {
    if (!mahi_manager_impl_->mahi_panel_widget_) {
      return nullptr;
    }
    return mahi_manager_impl_->mahi_panel_widget_->AsWidget();
  }

 protected:
  std::unique_ptr<FakeMahiProvider> CreateMahiProvider() {
    return std::make_unique<FakeMahiProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager());
  }

  // This instance is needed for setting up `ash_test_helper_`.
  // See //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;

  crosapi::TestCrosapiEnvironment crosapi_environment_;

  // Need this to set up `Shell` and display.
  AshTestHelper ash_test_helper_;
  std::unique_ptr<MahiManagerImpl> mahi_manager_impl_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      ash::switches::SetIgnoreMahiSecretKeyForTest();
};

TEST_F(MahiManagerImplTest, OpenPanel) {
  EXPECT_FALSE(GetMahiPanelWidget());

  auto* screen = display::Screen::GetScreen();
  auto display_id = screen->GetPrimaryDisplay().id();

  mahi_manager_impl_->OpenMahiPanel(display_id);

  // Widget should be created.
  auto* widget = GetMahiPanelWidget();
  EXPECT_TRUE(widget);

  // The widget should be in the same display as the given `display_id`.
  EXPECT_EQ(display_id,
            screen->GetDisplayNearestWindow(widget->GetNativeWindow()).id());
}

TEST_F(MahiManagerImplTest, OnContextMenuClickedSummary) {
  EXPECT_FALSE(GetMahiPanelWidget());

  auto* screen = display::Screen::GetScreen();
  auto display_id = screen->GetPrimaryDisplay().id();
  auto request = crosapi::mojom::MahiContextMenuRequest::New(
      display_id, MahiContextMenuActionType::kSummary, std::nullopt);
  mahi_manager_impl_->OnContextMenuClicked(std::move(request));

  // Widget should be created.
  auto* widget = GetMahiPanelWidget();
  EXPECT_TRUE(widget);
}

TEST_F(MahiManagerImplTest, OnContextMenuClickedSettings) {
  EXPECT_FALSE(GetMahiPanelWidget());

  auto* screen = display::Screen::GetScreen();
  auto display_id = screen->GetPrimaryDisplay().id();
  auto request = crosapi::mojom::MahiContextMenuRequest::New(
      display_id, MahiContextMenuActionType::kSettings, std::nullopt);
  mahi_manager_impl_->OnContextMenuClicked(std::move(request));

  EXPECT_FALSE(GetMahiPanelWidget());
}

class MahiManagerImplFeatureKeyTest : public testing::Test {
 public:
  MahiManagerImplFeatureKeyTest() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kMahiFeatureKey, "hello");
  }

 protected:
  views::Widget* GetMahiPanelWidget() {
    return mahi_manager_impl_.mahi_panel_widget_.get();
  }

  MahiManagerImpl mahi_manager_impl_;

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
};

TEST_F(MahiManagerImplFeatureKeyTest, DoesNotShowWidgetIfFeatureKeyIsWrong) {
  mahi_manager_impl_.OpenMahiPanel(/*display_id=*/0);

  EXPECT_THAT(GetMahiPanelWidget(), IsNull());
}

}  // namespace ash
