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
#include "base/metrics/histogram_macros.h"
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

// The opacity of the disabled item view.
constexpr float kDisabledAlpha = 0.38f;

// The insets within the contents view.
constexpr gfx::Insets kContentsInsets(/*vertical=*/4, /*horizontal=*/16);

// The size of the `DeleteButton`.
constexpr int kDeleteButtonSizeDip = 16;

}  // namespace

namespace ash {

ClipboardHistoryItemView::ContentsView::ContentsView(
    ClipboardHistoryItemView* container)
    : container_(container) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetBorder(views::CreateEmptyBorder(kContentsInsets));
}

ClipboardHistoryItemView::ContentsView::~ContentsView() = default;

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

// The view responding to mouse click or gesture tap events.
class ash::ClipboardHistoryItemView::MainButton : public views::Button {
 public:
  explicit MainButton(ClipboardHistoryItemView* container)
      : Button(), container_(container) {
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
    if (!container_->ShouldHighlight())
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

ClipboardHistoryItemView::DeleteButton::DeleteButton(
    ClipboardHistoryItemView* listener)
    : views::ImageButton(
          base::BindRepeating(&ClipboardHistoryItemView::ExecuteCommand,
                              base::Unretained(listener),
                              ClipboardHistoryUtil::kDeleteCommandId)) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(base::ASCIIToUTF16(std::string(GetClassName())));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(kDeleteButtonSizeDip, kDeleteButtonSizeDip));

  AshColorProvider::Get()->DecorateCloseButton(this, kDeleteButtonSizeDip,
                                               kCloseButtonIcon);
}

ClipboardHistoryItemView::DeleteButton::~DeleteButton() = default;

const char* ClipboardHistoryItemView::DeleteButton::GetClassName() const {
  return "DeleteButton";
}

// static
std::unique_ptr<ClipboardHistoryItemView>
ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
    const ClipboardHistoryItem& item,
    const ClipboardHistoryResourceManager* resource_manager,
    views::MenuItemView* container) {
  const auto display_format =
      ClipboardHistoryUtil::CalculateDisplayFormat(item.data());
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", display_format);
  switch (display_format) {
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kText:
      return std::make_unique<ClipboardHistoryTextItemView>(&item, container);
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kBitmap:
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kHtml:
      return std::make_unique<ClipboardHistoryBitmapItemView>(
          &item, resource_manager, container);
  }
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    views::MenuItemView* container)
    : clipboard_history_item_(clipboard_history_item), container_(container) {}

void ClipboardHistoryItemView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Ensures that MainButton is below any other child views.
  main_button_ = AddChildView(std::make_unique<MainButton>(this));
  main_button_->SetCallback(base::BindRepeating(
      [](ClipboardHistoryItemView* item, const ui::Event& event) {
        item->ExecuteCommand(item->CalculateCommandId(), event);
      },
      base::Unretained(this)));

  contents_view_ = AddChildView(CreateContentsView());

  subscription_ = container_->AddSelectedChangedCallback(base::BindRepeating(
      &ClipboardHistoryItemView::OnSelectionChanged, base::Unretained(this)));
}

void ClipboardHistoryItemView::OnSelectionChanged() {
  if (!container_->IsSelected()) {
    SetPseudoFocus(PseudoFocus::kEmpty);
    return;
  }

  // If the pseudo focus is moved from another item view via focus traversal,
  // `pseudo_focus_` is already up to date.
  if (pseudo_focus_ != PseudoFocus::kEmpty)
    return;

  InitiatePseudoFocus(/*reverse=*/false);
}

bool ClipboardHistoryItemView::AdvancePseudoFocus(bool reverse) {
  if (pseudo_focus_ == PseudoFocus::kEmpty) {
    InitiatePseudoFocus(reverse);
    return true;
  }

  // When the menu item is disabled, only the delete button is able to work.
  if (!container_->GetEnabled()) {
    DCHECK_EQ(PseudoFocus::kDeleteButton, pseudo_focus_);
    SetPseudoFocus(PseudoFocus::kEmpty);
    return false;
  }

  DCHECK(pseudo_focus_ == PseudoFocus::kMainButton ||
         pseudo_focus_ == PseudoFocus::kDeleteButton);
  int new_pseudo_focus = pseudo_focus_;
  bool move_focus_out = false;
  if (reverse) {
    --new_pseudo_focus;
    if (new_pseudo_focus == PseudoFocus::kEmpty)
      move_focus_out = true;
  } else {
    ++new_pseudo_focus;
    if (new_pseudo_focus == PseudoFocus::kMaxValue)
      move_focus_out = true;
  }

  if (move_focus_out) {
    SetPseudoFocus(PseudoFocus::kEmpty);
    return false;
  }

  SetPseudoFocus(static_cast<PseudoFocus>(new_pseudo_focus));
  return true;
}

void ClipboardHistoryItemView::RecordButtonPressedHistogram(
    bool is_delete_button) {
  if (is_delete_button) {
    ClipboardHistoryUtil::RecordClipboardHistoryItemDeleted(
        *clipboard_history_item_);
    return;
  }

  ClipboardHistoryUtil::RecordClipboardHistoryItemPasted(
      *clipboard_history_item_);
}

float ClipboardHistoryItemView::GetContentsOpacity() const {
  return container_->GetEnabled() ? 1.f : kDisabledAlpha;
}

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize() const {
  const int preferred_width =
      views::MenuConfig::instance().touchable_menu_width;
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void ClipboardHistoryItemView::ExecuteCommand(int command_id,
                                              const ui::Event& event) {
  RecordButtonPressedHistogram(/*is_delete_button=*/command_id ==
                               ClipboardHistoryUtil::kDeleteCommandId);
  views::MenuDelegate* delegate = container_->GetDelegate();
  DCHECK(delegate->IsCommandEnabled(command_id));
  container_->GetDelegate()->ExecuteCommand(command_id, event.flags());
}

int ClipboardHistoryItemView::CalculateCommandId() const {
  switch (pseudo_focus_) {
    case PseudoFocus::kMainButton:
      return container_->GetCommand();
    case PseudoFocus::kDeleteButton:
      return ClipboardHistoryUtil::kDeleteCommandId;
    case PseudoFocus::kEmpty:
    case PseudoFocus::kMaxValue:
      NOTREACHED();
      return -1;
  }
}

bool ClipboardHistoryItemView::ShouldHighlight() const {
  return pseudo_focus_ == PseudoFocus::kMainButton && container_->GetEnabled();
}

bool ClipboardHistoryItemView::ShouldShowDeleteButton() const {
  return (pseudo_focus_ == PseudoFocus::kMainButton && IsMouseHovered()) ||
         pseudo_focus_ == PseudoFocus::kDeleteButton;
}

void ClipboardHistoryItemView::InitiatePseudoFocus(bool reverse) {
  PseudoFocus target_pseudo_focus;
  if (!container_->GetEnabled() || reverse)
    target_pseudo_focus = PseudoFocus::kDeleteButton;
  else
    target_pseudo_focus = PseudoFocus::kMainButton;

  SetPseudoFocus(target_pseudo_focus);
}

void ClipboardHistoryItemView::SetPseudoFocus(PseudoFocus new_pseudo_focus) {
  if (pseudo_focus_ == new_pseudo_focus)
    return;

  pseudo_focus_ = new_pseudo_focus;
  contents_view_->delete_button()->SetVisible(ShouldShowDeleteButton());
  main_button_->SchedulePaint();
}

}  // namespace ash
