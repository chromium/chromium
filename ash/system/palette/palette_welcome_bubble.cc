// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_welcome_bubble.h"

#include <memory>

#include "ash/assistant/util/assistant_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_tray.h"
#include "base/command_line.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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
  METADATA_HEADER(WelcomeBubbleView, views::BubbleDialogDelegateView)

 public:
  WelcomeBubbleView(views::View* anchor, views::BubbleBorder::Arrow arrow)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    SetTitle(
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_TITLE));
    SetShowTitle(true);
    SetShowCloseButton(true);
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    set_close_on_deactivate(true);
    SetCanActivate(false);
    set_accept_events(true);
    set_parent_window(
        anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
            kShellWindowId_SettingBubbleContainer));
    views::BubbleDialogDelegateView::CreateBubble(this);
  }

  WelcomeBubbleView(const WelcomeBubbleView&) = delete;
  WelcomeBubbleView& operator=(const WelcomeBubbleView&) = delete;

  ~WelcomeBubbleView() override = default;

  void Init() override {
    SetUseDefaultFillLayout(true);
    views::Builder<views::BubbleDialogDelegateView>(this)
        .AddChild(views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_DESCRIPTION))
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetMultiLine(true)
                      .SizeToFit(kBubbleContentLabelPreferredWidthDp))
        .BuildChildren();
  }
};

BEGIN_METADATA(PaletteWelcomeBubble, WelcomeBubbleView)
END_METADATA

PaletteWelcomeBubble::PaletteWelcomeBubble(PaletteTray* tray) : tray_(tray) {
  Shell::Get()->session_controller()->AddObserver(this);
}

PaletteWelcomeBubble::~PaletteWelcomeBubble() {
  DisconnectObservers();
  Shell::Get()->session_controller()->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

// static
void PaletteWelcomeBubble::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShownPaletteWelcomeBubble, false);
}

void PaletteWelcomeBubble::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(bubble_view_ && bubble_view_->GetWidget() == widget);
  DisconnectObservers();
}

void PaletteWelcomeBubble::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
}

void PaletteWelcomeBubble::ShowIfNeeded() {
  if (!active_user_pref_service_)
    return;

  // The buble may interfere with some integration tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshNoNudges)) {
    return;
  }

  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  if (session_controller->IsRunningInAppMode()) {
    return;
  }

  std::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  if (user_type && (*user_type == user_manager::UserType::kGuest ||
                    *user_type == user_manager::UserType::kPublicAccount)) {
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
  if (bubble_view_) {
    bubble_view_->GetWidget()->Close();
    DisconnectObservers();
  }
}

void PaletteWelcomeBubble::DisconnectObservers() {
  if (bubble_view_) {
    bubble_view_->GetWidget()->RemoveObserver(this);
    bubble_view_ = nullptr;
  }
  Shell::Get()->RemovePreTargetHandler(this);
}

void PaletteWelcomeBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (bubble_view_ && event->type() == ui::EventType::kMousePressed &&
      event->target() != bubble_view_->GetWidget()->GetNativeView()) {
    bubble_view_->GetWidget()->Close();
  }
}

void PaletteWelcomeBubble::OnTouchEvent(ui::TouchEvent* event) {
  if (bubble_view_ && event->type() == ui::EventType::kTouchPressed &&
      event->target() != bubble_view_->GetWidget()->GetNativeView()) {
    bubble_view_->GetWidget()->Close();
  }
}

}  // namespace ash
