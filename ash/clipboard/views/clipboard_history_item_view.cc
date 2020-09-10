// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
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

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (!container_->IsSelected())
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

void ClipboardHistoryItemView::ContentsView::OnSelectionChanged() {
  // Update `delete_button_`'s visibility if the selection state switches.
  const bool is_selected = container_->IsSelected();
  if (is_selected != delete_button_->GetVisible())
    delete_button_->SetVisible(is_selected);
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
    const ClipboardHistoryResourceManager& resource_manager,
    views::MenuItemView* container) {
  switch (ClipboardHistoryUtil::CalculateMainFormat(item.data()).value()) {
    case ui::ClipboardInternalFormat::kBitmap:
      return std::make_unique<ClipboardHistoryBitmapItemView>(
          gfx::ImageSkia::CreateFrom1xBitmap(item.data().bitmap()), container);
    case ui::ClipboardInternalFormat::kHtml:
      return std::make_unique<ClipboardHistoryBitmapItemView>(
          *(resource_manager.GetImageModel(item).GetImage().ToImageSkia()),
          container);
    case ui::ClipboardInternalFormat::kText:
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

  subscription_ = container_->AddSelectedChangedCallback(base::BindRepeating(
      &ClipboardHistoryItemView::OnSelectionChanged, base::Unretained(this)));
}

bool ClipboardHistoryItemView::IsSelected() const {
  return container_->IsSelected();
}

void ClipboardHistoryItemView::OnSelectionChanged() {
  contents_view_->OnSelectionChanged();
  main_button_->SchedulePaint();
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
