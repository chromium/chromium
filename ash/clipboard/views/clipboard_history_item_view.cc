// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The insets within the contents view.
constexpr gfx::Insets kContentsInsets(/*vertical=*/4, /*horizontal=*/16);

// The size of the `DeleteButton`.
constexpr int kDeleteButtonSizeDip = 16;

// The view responding to mouse click or gesture tap events.
class MainButton : public views::Button {
 public:
  explicit MainButton(ash::ClipboardHistoryItemView* container)
      : Button(container), container_(container) {
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetAccessibleName(base::ASCIIToUTF16(std::string(GetClassName())));
  }
  MainButton(const MainButton& rhs) = delete;
  MainButton& operator=(const MainButton& rhs) = delete;
  ~MainButton() override = default;

 private:
  // views::Button:
  const char* GetClassName() const override { return "MainButton"; }

  void StateChanged(ButtonState old_state) override {
    container_->SelectionWillChange(IsSelected());
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (!IsSelected())
      return;

    // Highlight the background when the menu item is selected or pressed.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    const ui::NativeTheme::ColorId color_id =
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor;
    flags.setColor(GetNativeTheme()->GetSystemColor(color_id));

    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }

  bool IsSelected() const {
    // We should check both the mouse hovering state and the button state,
    // because:
    // (1) When the menu item is selected by the key traversal, the item is not
    // hovered by mouse.
    // (2) When the mouse moves within the bounds of DeleteButton, the menu item
    // is still selected but the button state is not
    // `ButtonState::STATE_HOVERED`.
    return IsMouseHovered() || GetState() == ButtonState::STATE_HOVERED ||
           GetState() == ButtonState::STATE_PRESSED;
  }

  // The parent view.
  ash::ClipboardHistoryItemView* const container_;
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView::ContentsView

ClipboardHistoryItemView::ContentsView::ContentsView(
    ClipboardHistoryItemView* container)
    : container_(container) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetBorder(views::CreateEmptyBorder(kContentsInsets));
}

ClipboardHistoryItemView::ContentsView::~ContentsView() = default;

void ClipboardHistoryItemView::ContentsView::SelectionWillChange(
    bool target_is_selected) {
  // Update |delete_button_|'s visiblity if the selection state switches.
  if (target_is_selected ^ delete_button_->GetVisible())
    delete_button_->SetVisible(target_is_selected);
}

void ClipboardHistoryItemView::ContentsView::InstallDeleteButton() {
  delete_button_ = CreateDeleteButton();
}

// Accepts the event only when |delete_button_| should be the handler.
bool ClipboardHistoryItemView::ContentsView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  if (!delete_button_->GetVisible())
    return false;

  gfx::RectF rect_in_delete_button(rect);
  ConvertRectToTarget(this, delete_button_, &rect_in_delete_button);
  return delete_button_->HitTestRect(
      gfx::ToEnclosedRect(rect_in_delete_button));
}

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView::DeleteButton

ClipboardHistoryItemView::DeleteButton::DeleteButton(
    views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(base::ASCIIToUTF16(std::string(GetClassName())));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(kDeleteButtonSizeDip, kDeleteButtonSizeDip));

  AshColorProvider::Get()->DecorateCloseButton(
      this, AshColorProvider::ButtonType::kCloseButtonWithSmallBase,
      kDeleteButtonSizeDip, kCloseButtonIcon);
}

ClipboardHistoryItemView::DeleteButton::~DeleteButton() = default;

const char* ClipboardHistoryItemView::DeleteButton::GetClassName() const {
  return "DeleteButton";
}

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView

// static
std::unique_ptr<ClipboardHistoryItemView>
ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
    const ClipboardHistoryItem& item,
    views::MenuItemView* container) {
  switch (ClipboardHistoryUtil::CalculateMainFormat(item.data()).value()) {
    case ui::ClipboardInternalFormat::kBitmap:
      return std::make_unique<ClipboardHistoryBitmapItemView>(item, container);
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kHtml:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
    case ui::ClipboardInternalFormat::kCustom:
      return std::make_unique<ClipboardHistoryTextItemView>(item, container);
  }
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    views::MenuItemView* container)
    : container_(container) {}

void ClipboardHistoryItemView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Ensures that MainButton is below any other child views.
  main_button_ = AddChildView(std::make_unique<MainButton>(this));

  contents_view_ = AddChildView(CreateContentsView());
}

void ClipboardHistoryItemView::SelectionWillChange(bool target_is_selected) {
  contents_view_->SelectionWillChange(target_is_selected);
}

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize() const {
  const int preferred_width =
      views::MenuConfig::instance().touchable_menu_width;
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void ClipboardHistoryItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK(sender == contents_view_->delete_button() || sender == main_button_);
  const int command_id = sender == contents_view_->delete_button()
                             ? ClipboardHistoryUtil::kDeleteCommandId
                             : container_->GetCommand();
  container_->GetDelegate()->ExecuteCommand(command_id, event.flags());
}

}  // namespace ash
