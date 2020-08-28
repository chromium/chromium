// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
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
    if (GetState() != ButtonState::STATE_HOVERED &&
        GetState() != ButtonState::STATE_NORMAL) {
      return;
    }

    container_->SelectionWillChange(GetState() == ButtonState::STATE_HOVERED);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetState() != ButtonState::STATE_HOVERED &&
        GetState() != ButtonState::STATE_PRESSED) {
      return;
    }

    // Highlight the background when the menu item is under selection.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    const ui::NativeTheme::ColorId color_id =
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor;
    flags.setColor(GetNativeTheme()->GetSystemColor(color_id));

    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
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
  return delete_button_->GetVisible() && delete_button_->HitTestRect(rect);
}

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryItemView::DeleteButton

ClipboardHistoryItemView::DeleteButton::DeleteButton(
    views::ButtonListener* listener)
    : views::ImageButton(listener) {
  const gfx::ImageSkia icon_image =
      gfx::CreateVectorIcon(kDeleteIcon, kDeleteButtonSizeDip, SK_ColorBLACK);
  SetImage(views::ImageButton::STATE_NORMAL, icon_image);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(base::ASCIIToUTF16(std::string(GetClassName())));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
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
  AddChildView(std::make_unique<MainButton>(this));

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
  container_->GetDelegate()->ExecuteCommand(container_->GetCommand(),
                                            event.flags());
}

}  // namespace ash
