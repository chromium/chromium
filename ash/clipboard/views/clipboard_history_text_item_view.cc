// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "base/metrics/histogram_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// The preferred height for the label.
constexpr int kLabelPreferredHeight = 32;

// The margins of the delete button.
constexpr gfx::Insets kDeleteButtonMargins =
    gfx::Insets(/*top=*/0, /*left=*/8, /*bottom=*/0, /*right=*/4);
}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryTextItemView::TextContentsView

class ClipboardHistoryTextItemView::TextContentsView
    : public ClipboardHistoryTextItemView::ContentsView {
 public:
  explicit TextContentsView(ClipboardHistoryItemView* container)
      : ContentsView(container) {}
  TextContentsView(const TextContentsView& rhs) = delete;
  TextContentsView& operator=(const TextContentsView& rhs) = delete;
  ~TextContentsView() override = default;

 private:
  // ContentsView:
  DeleteButton* CreateDeleteButton() override {
    auto delete_button = std::make_unique<DeleteButton>(container());
    delete_button->SetVisible(false);
    delete_button->SetProperty(views::kMarginsKey, kDeleteButtonMargins);
    return AddChildView(std::move(delete_button));
  }

  const char* GetClassName() const override {
    return "ClipboardHistoryTextItemView::TextContentsView";
  }
};

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryTextItemView

ClipboardHistoryTextItemView::ClipboardHistoryTextItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(clipboard_history_item, container),
      text_(Shell::Get()
                ->clipboard_history_controller()
                ->resource_manager()
                ->GetLabel(*clipboard_history_item)) {}

ClipboardHistoryTextItemView::~ClipboardHistoryTextItemView() = default;

std::unique_ptr<ClipboardHistoryTextItemView::ContentsView>
ClipboardHistoryTextItemView::CreateContentsView() {
  auto contents_view = std::make_unique<TextContentsView>(this);
  auto* layout =
      contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  label_ = contents_view->AddChildView(std::make_unique<views::Label>(text_));
  label_->SetPreferredSize(gfx::Size(INT_MAX, kLabelPreferredHeight));
  label_->SetFontList(views::style::GetFont(views::style::CONTEXT_TOUCH_MENU,
                                            views::style::STYLE_PRIMARY));
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAutoColorReadabilityEnabled(/*enabled=*/false);
  layout->SetFlexForView(label_, /*flex_weight=*/1);

  contents_view->InstallDeleteButton();

  return contents_view;
}

base::string16 ClipboardHistoryTextItemView::GetAccessibleName() const {
  return text_;
}

const char* ClipboardHistoryTextItemView::GetClassName() const {
  return "ClipboardHistoryTextItemView";
}

void ClipboardHistoryTextItemView::OnThemeChanged() {
  // Use the light mode as default because the light mode is the default mode of
  // the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is fixed.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  ClipboardHistoryItemView::OnThemeChanged();

  // Calculate the text color.
  const auto color_type =
      IsItemEnabled() ? AshColorProvider::ContentLayerType::kTextColorPrimary
                      : AshColorProvider::ContentLayerType::kTextColorSecondary;
  const SkColor text_color =
      AshColorProvider::Get()->GetContentLayerColor(color_type);

  label_->SetEnabledColor(text_color);
  label_->SchedulePaint();
}

}  // namespace ash
