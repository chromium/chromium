// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

constexpr char kFakeAppId[] = "FakeAppId";
constexpr char kFakeAppPackageName[] = "FakeAppPackageName";
constexpr int kLaunchAppShortcutFirst = 100;
constexpr int kLaunchAppShortcutLast = 199;

}  // namespace

class ArcAppShortcutsMenuBuilderTest : public testing::Test {
 public:
  ArcAppShortcutsMenuBuilderTest() = default;

  ArcAppShortcutsMenuBuilderTest(const ArcAppShortcutsMenuBuilderTest&) =
      delete;
  ArcAppShortcutsMenuBuilderTest& operator=(
      const ArcAppShortcutsMenuBuilderTest&) = delete;

  ~ArcAppShortcutsMenuBuilderTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());
    IconDecodeRequest::DisableSafeDecodingForTesting();
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ArcAppShortcutsMenuBuilderTest, Basic) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu;
  auto simple_menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  const std::u16string first_item_label = u"FirstItemLabel";
  simple_menu_model->AddItem(1, first_item_label);
  auto arc_app_shortcuts_menu_builder =
      std::make_unique<ArcAppShortcutsMenuBuilder>(
          profile(), kFakeAppId, display::kInvalidDisplayId,
          kLaunchAppShortcutFirst, kLaunchAppShortcutLast);
  arc_app_shortcuts_menu_builder->BuildMenu(
      kFakeAppPackageName, std::move(simple_menu_model),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> returned_menu) {
            menu = std::move(returned_menu);
            run_loop.Quit();
          }));
  run_loop.Run();

  DCHECK(menu);
  size_t i = 0;
  EXPECT_EQ(first_item_label, menu->GetLabelAt(i++));
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(i++));
  // There is a separator between each app shortcut.
  for (size_t shortcut_index = 0; i < menu->GetItemCount(); ++i) {
    EXPECT_EQ("ShortLabel " + base::NumberToString(shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(i++)));
    if (i < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(i));
  }
}

}  // namespace arc
