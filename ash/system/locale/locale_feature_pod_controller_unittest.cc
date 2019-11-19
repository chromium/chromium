// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_feature_pod_controller.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/locale_update_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace {

class LocaleFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  LocaleFeaturePodControllerTest() = default;
  ~LocaleFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    controller_ =
        std::make_unique<LocaleFeaturePodController>(tray_controller_.get());
  }

  void TearDown() override {
    controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<LocaleFeaturePodController> controller_;

 private:
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(LocaleFeaturePodControllerTest);
};

TEST_F(LocaleFeaturePodControllerTest, ButtonVisibility) {
  constexpr char kDefaultLocaleIsoCode[] = "en-US";
  // The button is invisible if the locale list is unset.
  std::unique_ptr<FeaturePodButton> button;
  button.reset(controller_->CreateButton());
  EXPECT_FALSE(button->GetVisible());

  // The button is invisible if the locale list is empty.
  Shell::Get()->system_tray_model()->SetLocaleList({}, kDefaultLocaleIsoCode);
  button.reset(controller_->CreateButton());
  EXPECT_FALSE(button->GetVisible());

  // The button is visible if the locale list is non-empty.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back(kDefaultLocaleIsoCode,
                           base::UTF8ToUTF16("English (United States)"));
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   kDefaultLocaleIsoCode);
  button.reset(controller_->CreateButton());
  EXPECT_TRUE(button->GetVisible());
}

}  // namespace
}  // namespace ash
