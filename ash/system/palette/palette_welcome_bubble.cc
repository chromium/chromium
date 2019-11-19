// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_welcome_bubble.h"

#include <memory>

#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_tray.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Width of the bubble content label size.
constexpr int kBubbleContentLabelPreferredWidthDp = 380;

}  // namespace

// Controlled by PaletteWelcomeBubble and anchored to a PaletteTray.
class PaletteWelcomeBubble::WelcomeBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  WelcomeBubbleView(views::View* anchor, views::BubbleBorder::Arrow arrow)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    set_close_on_deactivate(true);
    SetCanActivate(false);
    set_accept_events(true);
    set_parent_window(
        anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
            kShellWindowId_SettingBubbleContainer));
    views::BubbleDialogDelegateView::CreateBubble(this);
  }

  ~WelcomeBubbleView() override = default;

  // ui::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override {
    return l10n_util::GetStringUTF16(IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_TITLE);
  }

  bool ShouldShowWindowTitle() const override { return true; }

  bool ShouldShowCloseButton() const override { return true; }

  void Init() override {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* label = new views::Label(l10n_util::GetStringUTF16(
        assistant::util::IsGoogleDevice()
            ? IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_WITH_ASSISTANT_DESCRIPTION
            : IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_DESCRIPTION));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    label->SizeToFit(kBubbleContentLabelPreferredWidthDp);
    AddChildView(label);
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  // views::View:
  const char* GetClassName() const override { return "WelcomeBubbleView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(WelcomeBubbleView);
};

PaletteWelcomeBubble::PaletteWelcomeBubble(PaletteTray* tray) : tray_(tray) {
  Shell::Get()->session_controller()->AddObserver(this);
}

PaletteWelcomeBubble::~PaletteWelcomeBubble() {
  if (bubble_view_) {
    bubble_view_->GetWidget()->RemoveObserver(this);
    Shell::Get()->RemovePreTargetHandler(this);
  }
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PaletteWelcomeBubble::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShownPaletteWelcomeBubble, false);
}

void PaletteWelcomeBubble::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;
  Shell::Get()->RemovePreTargetHandler(this);
}

void PaletteWelcomeBubble::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
}

void PaletteWelcomeBubble::ShowIfNeeded() {
  if (!active_user_pref_service_)
    return;

  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  base::Optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  if (user_type && (*user_type == user_manager::USER_TYPE_GUEST ||
                    *user_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT)) {
    return;
  }

  if (!HasBeenShown())
    Show();
}

bool PaletteWelcomeBubble::HasBeenShown() const {
  return active_user_pref_service_ && active_user_pref_service_->GetBoolean(
                                          prefs::kShownPaletteWelcomeBubble);
}

void PaletteWelcomeBubble::MarkAsShown() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kShownPaletteWelcomeBubble,
                                        true);
}

views::View* PaletteWelcomeBubble::GetBubbleViewForTesting() {
  return bubble_view_;
}

void PaletteWelcomeBubble::Show() {
  if (!bubble_view_) {
    DCHECK(tray_);
    bubble_view_ =
        new WelcomeBubbleView(tray_, views::BubbleBorder::BOTTOM_RIGHT);
  }
  MarkAsShown();
  bubble_view_->GetWidget()->Show();
  bubble_view_->GetWidget()->AddObserver(this);
  Shell::Get()->AddPreTargetHandler(this);
}

void PaletteWelcomeBubble::Hide() {
  if (bubble_view_)
    bubble_view_->GetWidget()->Close();
}

void PaletteWelcomeBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (bubble_view_ && event->type() == ui::ET_MOUSE_PRESSED &&
      event->target() != bubble_view_->GetWidget()->GetNativeView()) {
    bubble_view_->GetWidget()->Close();
  }
}

void PaletteWelcomeBubble::OnTouchEvent(ui::TouchEvent* event) {
  if (bubble_view_ && event->type() == ui::ET_TOUCH_PRESSED &&
      event->target() != bubble_view_->GetWidget()->GetNativeView()) {
    bubble_view_->GetWidget()->Close();
  }
}

}  // namespace ash
