// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/locale_update_controller.h"
#include "ash/shell.h"
#include "ash/style/rounded_container.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class LocaleDetailedViewTest : public AshTestBase {
 public:
  LocaleDetailedViewTest() = default;

  void CreateDetailedView() {
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);  // Ensure widget can contain whole view.
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    detailed_view_ = widget_->SetContentsView(
        std::make_unique<LocaleDetailedView>(delegate_.get()));
  }

  void CloseDetailedView() {
    widget_.reset();
    detailed_view_ = nullptr;
    delegate_.reset();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<DetailedViewDelegate> delegate_;
  raw_ptr<LocaleDetailedView, DanglingUntriaged> detailed_view_ = nullptr;
};

TEST_F(LocaleDetailedViewTest, CreatesRoundedContainer) {
  // Setup two locales in the locale list.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back("en-US", u"English (United States)");
  locale_list.emplace_back("fr-FR", u"French (France)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   "en-US");

  CreateDetailedView();

  // The scroll content contains one child, a RoundedContainer.
  views::View* scroll_content = detailed_view_->GetScrollContentForTest();
  ASSERT_TRUE(scroll_content);
  ASSERT_EQ(scroll_content->children().size(), 1u);
  views::View* rounded_container = scroll_content->children()[0];
  ASSERT_TRUE(rounded_container);
  EXPECT_TRUE(views::IsViewClass<RoundedContainer>(rounded_container));

  // The RoundedContainer has two children (for the two locales).
  EXPECT_EQ(rounded_container->children().size(), 2u);

  CloseDetailedView();
}

TEST_F(LocaleDetailedViewTest, AccessibleCheckedStateChange) {
  CloseDetailedView();
  // Setup two locales in the locale list.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back("en-US", u"English (United States)");
  locale_list.emplace_back("fr-FR", u"French (France)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   "en-US");

  CreateDetailedView();

  // The scroll content contains one child, a RoundedContainer.
  views::View* scroll_content = detailed_view_->GetScrollContentForTest();
  views::View* rounded_container = scroll_content->children()[0];
  views::View* local_item_view_checked = rounded_container->children()[0];
  views::View* local_item_view_unchecked = rounded_container->children()[1];

  ui::AXNodeData data;
  local_item_view_checked->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  local_item_view_unchecked->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  CloseDetailedView();
}

}  // namespace
}  // namespace ash
