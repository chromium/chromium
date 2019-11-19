// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/toast_dialog_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace lock_screen_apps {

namespace {

constexpr int kDialogWidthDp = 292;

constexpr int kDialogMessageMarginTopDp = 0;
constexpr int kDialogMessageMarginStartDp = 16;
constexpr int kDialogMessageMarginBottomDp = 18;
constexpr int kDialogMessageMarginEndDp = 12;
constexpr int kDialogMessageLineHeightDp = 20;

constexpr int kDialogTitleMarginTopDp = 14;
constexpr int kDialogTitleMarginStartDp = 16;
constexpr int kDialogTitleMarginBottomDp = 5;
constexpr int kDialogTitleMarginEndDp = 0;

}  // namespace

ToastDialogView::ToastDialogView(const base::string16& app_name,
                                 base::OnceClosure dismissed_callback)
    : app_name_(app_name), dismissed_callback_(std::move(dismissed_callback)) {
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::LOCK_SCREEN_NOTE_APP_TOAST);

  SetArrow(views::BubbleBorder::NONE);
  set_margins(
      gfx::Insets(kDialogMessageMarginTopDp, kDialogMessageMarginStartDp,
                  kDialogMessageMarginBottomDp, kDialogMessageMarginEndDp));
  set_title_margins(
      gfx::Insets(kDialogTitleMarginTopDp, kDialogTitleMarginStartDp,
                  kDialogTitleMarginBottomDp, kDialogTitleMarginEndDp));
  set_shadow(views::BubbleBorder::SMALL_SHADOW);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_LOCK_SCREEN_NOTE_APP_TOAST_DIALOG_MESSAGE, app_name_));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(138, 0, 0, 0));
  label->SetLineHeight(kDialogMessageLineHeightDp);
  label->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  label->SetPreferredSize(
      gfx::Size(kDialogWidthDp, label->GetHeightForWidth(kDialogWidthDp)));
  label->SizeToPreferredSize();

  AddChildView(label);
}

ToastDialogView::~ToastDialogView() = default;

void ToastDialogView::Show() {
  views::Widget::InitParams params =
      GetDialogWidgetInitParams(this, nullptr, nullptr, gfx::Rect());

  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_SettingBubbleContainer);

  views::Widget* widget = new views::Widget;  // owned by native widget
  widget->Init(std::move(params));
  widget->AddObserver(this);
  widget->Show();
}

ui::ModalType ToastDialogView::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 ToastDialogView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_NOTE_APP_TOAST_DIALOG_TITLE,
                                    app_name_);
}

bool ToastDialogView::Close() {
  if (!dismissed_callback_.is_null())
    std::move(dismissed_callback_).Run();
  return true;
}

void ToastDialogView::AddedToWidget() {
  std::unique_ptr<views::Label> title =
      views::BubbleFrameView::CreateDefaultTitleLabel(GetWindowTitle());
  title->SetFontList(views::Label::GetDefaultFontList().Derive(
      3, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  GetBubbleFrameView()->SetTitleView(std::move(title));
}

int ToastDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ToastDialogView::ShouldShowCloseButton() const {
  return true;
}

}  // namespace lock_screen_apps
