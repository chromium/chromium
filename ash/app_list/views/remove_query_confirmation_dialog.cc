// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include "ash/app_list/views/search_box_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_client_view.h"

namespace ash {

namespace {

constexpr int kDialogWidth = 320;
constexpr int kDialogYOffset = 32;

}  // namespace

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    const base::string16& query,
    RemovalConfirmationCallback confirm_callback,
    int event_flags,
    ContentsView* contents_view)
    : confirm_callback_(std::move(confirm_callback)),
      event_flags_(event_flags),
      contents_view_(contents_view) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  views::Label* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS, query));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  AddChildView(label);

  contents_view_->AddSearchBoxUpdateObserver(this);
}

RemoveQueryConfirmationDialog::~RemoveQueryConfirmationDialog() {
  contents_view_->RemoveSearchBoxUpdateObserver(this);
}

void RemoveQueryConfirmationDialog::Show(gfx::NativeWindow parent) {
  views::DialogDelegate::CreateDialogWidget(this, nullptr, parent);
  UpdateBounds();
  GetWidget()->Show();
}

const char* RemoveQueryConfirmationDialog::GetClassName() const {
  return "RemoveQueryConfirmationDialog";
}

base::string16 RemoveQueryConfirmationDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE);
}

ui::ModalType RemoveQueryConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool RemoveQueryConfirmationDialog::ShouldShowCloseButton() const {
  return false;
}

bool RemoveQueryConfirmationDialog::Accept() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(true, event_flags_);

  return true;
}

bool RemoveQueryConfirmationDialog::Cancel() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(false, event_flags_);

  return true;
}

gfx::Size RemoveQueryConfirmationDialog::CalculatePreferredSize() const {
  const int default_width = kDialogWidth;
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

void RemoveQueryConfirmationDialog::OnSearchBoxBoundsUpdated() {
  UpdateBounds();
}

void RemoveQueryConfirmationDialog::OnSearchBoxClearAndDeactivated() {
  // In tablet mode, when the user opens uber tray, the search box will be
  // cleared and deactivated while app list switches to full app mode. Close
  // this dialog when receiving such notification.
  // Note: When the dialog is closed, the focus manager will restore
  // the focus to the previously focused view, i.e., SearchBoxView's
  // text field, which will lead to the opening of virtual keyboard. In order to
  // avoid this, we temporarily clear the stored focus view before closing
  // the dialog, and restore it back right after.
  views::FocusManager* focus_manager =
      contents_view_->GetSearchBoxView()->GetWidget()->GetFocusManager();
  views::View* strored_focus_view = focus_manager->GetStoredFocusView();
  focus_manager->SetStoredFocusView(nullptr);
  GetDialogClientView()->CancelWindow();
  focus_manager->SetStoredFocusView(strored_focus_view);
}

void RemoveQueryConfirmationDialog::UpdateBounds() {
  // Calculate confirmation dialog's origin in screen coordinates.
  gfx::Rect anchor_rect =
      contents_view_->GetSearchBoxView()->GetBoundsInScreen();
  gfx::Point origin(anchor_rect.CenterPoint().x() - kDialogWidth / 2,
                    anchor_rect.y() + kDialogYOffset);

  views::Widget* widget = GetWidget();
  DCHECK(widget);
  gfx::Rect widget_rect = widget->GetWindowBoundsInScreen();
  widget_rect.set_origin(origin);
  widget->SetBounds(widget_rect);
}

}  // namespace ash
