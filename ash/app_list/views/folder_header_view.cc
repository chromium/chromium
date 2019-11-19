// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_header_view.h"

#include <algorithm>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/painter.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

namespace {

constexpr int kMaxFolderNameWidth = 204;
constexpr SkColor kFolderNameColor = SkColorSetARGB(138, 0, 0, 0);
constexpr SkColor kFolderTitleHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

}  // namespace

class FolderHeaderView::FolderNameView : public views::Textfield,
                                         public views::ViewTargeterDelegate {
 public:
  explicit FolderNameView(FolderHeaderView* folder_header_view)
      : folder_header_view_(folder_header_view) {
    DCHECK(folder_header_view_);
    SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }

  ~FolderNameView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kMaxFolderNameWidth,
                     AppListConfig::instance().folder_header_height());
  }

  void OnFocus() override {
    SetText(base::UTF8ToUTF16(folder_header_view_->folder_item_->name()));
    starting_name_ = GetText();
    folder_header_view_->previous_folder_name_ = starting_name_;
    SelectAll(false);
    Textfield::OnFocus();
  }

  void OnBlur() override {
    // Collapse whitespace when FolderNameView loses focus.
    folder_header_view_->ContentsChanged(
        this, base::CollapseWhitespace(GetText(), false));

    // Ensure folder name is truncated when FolderNameView loses focus.
    SetText(folder_header_view_->GetElidedFolderName(
        base::UTF8ToUTF16(folder_header_view_->folder_item_->name())));

    // Record metric each time a folder is renamed.
    if (GetText() != starting_name_) {
      if (folder_header_view_->is_tablet_mode()) {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.TabletMode",
                                 GetText().length());
      } else {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.ClamshellMode",
                                 GetText().length());
      }
    }

    Textfield::OnBlur();
  }

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    gfx::Rect textfield_bounds = target->GetLocalBounds();
    int horizontal_padding = -(textfield_bounds.height() * 1.5);
    textfield_bounds.Inset(gfx::Insets(0, horizontal_padding));
    return textfield_bounds.Intersects(rect);
  }

 private:
  // The parent FolderHeaderView, owns this.
  FolderHeaderView* folder_header_view_;

  // Name of the folder when FolderNameView is focused, used to track folder
  // rename metric.
  base::string16 starting_name_;

  DISALLOW_COPY_AND_ASSIGN(FolderNameView);
};

FolderHeaderView::FolderHeaderView(FolderHeaderViewDelegate* delegate)
    : folder_item_(nullptr),
      folder_name_view_(new FolderNameView(this)),
      folder_name_placeholder_text_(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER)),
      delegate_(delegate),
      folder_name_visible_(true),
      is_tablet_mode_(false) {
  folder_name_view_->set_placeholder_text_color(kFolderTitleHintTextColor);
  folder_name_view_->SetPlaceholderText(folder_name_placeholder_text_);
  folder_name_view_->SetBorder(views::NullBorder());

  // Make folder name font size 14px.
  folder_name_view_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(2));
  folder_name_view_->SetBackgroundColor(SK_ColorTRANSPARENT);
  folder_name_view_->SetTextColor(kFolderNameColor);
  folder_name_view_->set_controller(this);
  AddChildView(folder_name_view_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FolderHeaderView::~FolderHeaderView() {
  if (folder_item_)
    folder_item_->RemoveObserver(this);
}

void FolderHeaderView::SetFolderItem(AppListFolderItem* folder_item) {
  if (folder_item_)
    folder_item_->RemoveObserver(this);

  folder_item_ = folder_item;
  if (!folder_item_)
    return;
  folder_item_->AddObserver(this);

  folder_name_view_->SetEnabled(folder_item_->folder_type() !=
                                AppListFolderItem::FOLDER_TYPE_OEM);

  Update();
}

void FolderHeaderView::UpdateFolderNameVisibility(bool visible) {
  folder_name_visible_ = visible;
  Update();
  SchedulePaint();
}

void FolderHeaderView::OnFolderItemRemoved() {
  folder_item_ = nullptr;
}

void FolderHeaderView::SetTextFocus() {
  folder_name_view_->RequestFocus();
}

bool FolderHeaderView::HasTextFocus() const {
  return folder_name_view_->HasFocus();
}

void FolderHeaderView::Update() {
  if (!folder_item_)
    return;

  folder_name_view_->SetVisible(folder_name_visible_);
  if (folder_name_visible_) {
    base::string16 folder_name = base::UTF8ToUTF16(folder_item_->name());
    base::string16 elided_folder_name = GetElidedFolderName(folder_name);
    folder_name_view_->SetText(elided_folder_name);
    UpdateFolderNameAccessibleName();
  }

  Layout();
}

void FolderHeaderView::UpdateFolderNameAccessibleName() {
  // Sets |folder_name_view_|'s accessible name to the placeholder text if
  // |folder_name_view_| is blank; otherwise, clear the accessible name, the
  // accessible state's value is set to be folder_name_view_->GetText() by
  // TextField.
  base::string16 accessible_name = folder_name_view_->GetText().empty()
                                       ? folder_name_placeholder_text_
                                       : base::string16();
  folder_name_view_->SetAccessibleName(accessible_name);
}

const base::string16& FolderHeaderView::GetFolderNameForTest() {
  return folder_name_view_->GetText();
}

void FolderHeaderView::SetFolderNameForTest(const base::string16& name) {
  folder_name_view_->SetText(name);
}

bool FolderHeaderView::IsFolderNameEnabledForTest() const {
  return folder_name_view_->GetEnabled();
}

gfx::Size FolderHeaderView::CalculatePreferredSize() const {
  return gfx::Size(kMaxFolderNameWidth,
                   folder_name_view_->GetPreferredSize().height());
}

const char* FolderHeaderView::GetClassName() const {
  return "FolderHeaderView";
}

void FolderHeaderView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  Update();
}

views::View* FolderHeaderView::GetFolderNameViewForTest() const {
  return folder_name_view_;
}

int FolderHeaderView::GetMaxFolderNameWidth() const {
  return kMaxFolderNameWidth;
}

base::string16 FolderHeaderView::GetElidedFolderName(
    const base::string16& folder_name) const {
  // Enforce the maximum folder name length.
  base::string16 name =
      folder_name.substr(0, AppListConfig::instance().max_folder_name_chars());

  // Get maximum text width for fitting into |folder_name_view_|.
  int text_width = std::min(GetMaxFolderNameWidth(), width()) -
                   folder_name_view_->GetCaretBounds().width() -
                   folder_name_view_->GetInsets().width();
  base::string16 elided_name = gfx::ElideText(
      name, folder_name_view_->GetFontList(), text_width, gfx::ELIDE_TAIL);
  return elided_name;
}

void FolderHeaderView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect text_bounds(rect);
  base::string16 text = folder_item_ && !folder_item_->name().empty()
                            ? base::UTF8ToUTF16(folder_item_->name())
                            : folder_name_placeholder_text_;
  int text_width =
      gfx::Canvas::GetStringWidth(text, folder_name_view_->GetFontList()) +
      folder_name_view_->GetCaretBounds().width() +
      folder_name_view_->GetInsets().width();
  text_width = std::min(text_width, GetMaxFolderNameWidth());
  text_bounds.set_x(std::max(0, rect.x() + (rect.width() - text_width) / 2));

  // The width of the text field should always be the maximum length possible,
  // to prevent the touch target from resizing with the text. The width should
  // also stay within the FolderHeaderView bounds.
  text_bounds.set_width(std::min(rect.width(), GetMaxFolderNameWidth()));

  text_bounds.ClampToCenteredSize(gfx::Size(
      text_bounds.width(), folder_name_view_->GetPreferredSize().height()));
  folder_name_view_->SetBoundsRect(text_bounds);
}

void FolderHeaderView::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
  // Temporarily remove from observer to ignore data change caused by us.
  if (!folder_item_)
    return;

  folder_item_->RemoveObserver(this);
  // Enforce the maximum folder name length in UI.
  if (new_contents.length() >
      AppListConfig::instance().max_folder_name_chars()) {
    folder_name_view_->SetText(previous_folder_name_.value());
    sender->SetSelectedRange(gfx::Range(previous_cursor_position_.value(),
                                        previous_cursor_position_.value()));
  } else {
    previous_folder_name_ = new_contents;
    delegate_->SetItemName(folder_item_, base::UTF16ToUTF8(new_contents));
  }

  folder_item_->AddObserver(this);

  UpdateFolderNameAccessibleName();

  Layout();
}

bool FolderHeaderView::HandleKeyEvent(views::Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.key_code() == ui::VKEY_RETURN &&
      key_event.type() == ui::ET_KEY_PRESSED) {
    delegate_->GiveBackFocusToSearchBox();
    delegate_->NavigateBack(folder_item_, key_event);
    return true;
  }
  if (!IsUnhandledLeftRightKeyEvent(key_event))
    return false;
  return ProcessLeftRightKeyTraversalForTextfield(folder_name_view_, key_event);
}

void FolderHeaderView::OnBeforeUserAction(views::Textfield* sender) {
  previous_cursor_position_ = sender->GetCursorPosition();
}

void FolderHeaderView::ItemNameChanged() {
  Update();
}

void FolderHeaderView::SetPreviousCursorPositionForTest(
    const size_t cursor_position) {
  previous_cursor_position_ = cursor_position;
}

void FolderHeaderView::SetPreviousFolderNameForTest(
    const base::string16& previous_name) {
  previous_folder_name_ = previous_name;
}

}  // namespace ash
