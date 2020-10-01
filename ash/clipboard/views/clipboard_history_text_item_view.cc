// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// The preferred height for the label.
constexpr int kLabelPreferredHeight = 16;

// The margins of the delete button.
constexpr gfx::Insets kDeleteButtonMargins =
    gfx::Insets(/*top=*/0, /*left=*/0, /*bottom=*/0, /*right=*/4);
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
    auto delete_button = std::make_unique<DeleteButton>(container_);
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

const char* ClipboardHistoryTextItemView::GetClassName() const {
  return "ClipboardHistoryTextItemView";
}

std::unique_ptr<ClipboardHistoryTextItemView::ContentsView>
ClipboardHistoryTextItemView::CreateContentsView() {
  auto contents_view = std::make_unique<TextContentsView>(this);
  auto* layout =
      contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* label =
      contents_view->AddChildView(std::make_unique<views::Label>(text_));
  label->SetPreferredSize(gfx::Size(INT_MAX, kLabelPreferredHeight));
  label->SetFontList(views::MenuConfig::instance().font_list);
  label->SetMultiLine(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(
      SkColorSetA(SK_ColorBLACK, 0xFF * GetContentsOpacity()));
  layout->SetFlexForView(label, /*flex_weights=*/1);

  contents_view->InstallDeleteButton();

  return contents_view;
}

}  // namespace ash
