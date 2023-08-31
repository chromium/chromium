// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Margins between containers in the detailed view.
constexpr auto kContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);

}  // namespace

FocusModeDetailedView::FocusModeDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  // TODO(b/288975135): update with official string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_FOCUS_MODE);
  CreateScrollableList();

  // TODO(b/286932057): remove border inset and add row toggle UI.
  toggle_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));
  toggle_view_->SetBorderInsets(gfx::Insets::VH(32, 0));

  // TODO(b/286931575): remove border inset and add Timer UI.
  timer_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  timer_view_->SetBorderInsets(gfx::Insets::VH(56, 0));
  timer_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  // TODO(b/286931806): remove border inset and add Focus Scene UI.
  scene_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  scene_view_->SetBorderInsets(gfx::Insets::VH(100, 0));
  scene_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  CreateDoNotDisturbContainer();

  scroll_content()->SizeToPreferredSize();

  FocusModeController::Get()->AddObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);
}

FocusModeDetailedView::~FocusModeDetailedView() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  FocusModeController::Get()->RemoveObserver(this);
}

void FocusModeDetailedView::OnQuietModeChanged(bool in_quiet_mode) {
  // When focus mode is not in a session, the state of the
  // `do_not_disturb_toggle_button_` will represent the initial state for the
  // next focus session. Once the focus mode session begins, this button should
  // be reflective of the actual system do not disturb state.
  if (FocusModeController::Get()->in_focus_session()) {
    do_not_disturb_toggle_button_->SetIsOn(in_quiet_mode);
  }
}

void FocusModeDetailedView::OnFocusModeChanged(bool in_focus_session) {
  do_not_disturb_toggle_button_->SetIsOn(
      FocusModeController::Get()->turn_on_do_not_disturb());
}

void FocusModeDetailedView::CreateDoNotDisturbContainer() {
  do_not_disturb_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));
  do_not_disturb_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  HoverHighlightView* toggle_row = do_not_disturb_view_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row->SetFocusBehavior(View::FocusBehavior::NEVER);

  // Create the do not disturb icon and its label.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemTrayDoNotDisturbIcon, cros_tokens::kCrosSysOnSurface));
  toggle_row->AddViewAndLabel(
      std::move(icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE_DO_NOT_DISTURB));
  toggle_row->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *toggle_row->text_label());

  // Create the toggle button for do not disturb.
  auto toggle = std::make_unique<Switch>(
      base::BindRepeating(&FocusModeDetailedView::OnDoNotDisturbToggleClicked,
                          weak_factory_.GetWeakPtr()));
  toggle->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DO_NOT_DISTURB));
  auto* controller = FocusModeController::Get();

  // The state of the toggle button is used for showing whether the
  // do-not-disturb mode is on/off on the device while in a focus session.
  // However, if there is no focus session running, it's used for representing
  // if the user wants to turn on/off the do not disturb when the next focus
  // session is started.
  toggle->SetIsOn(controller->in_focus_session()
                      ? message_center::MessageCenter::Get()->IsQuietMode()
                      : controller->turn_on_do_not_disturb());
  do_not_disturb_toggle_button_ = toggle.get();
  toggle_row->AddRightView(toggle.release());

  // TODO(hongyulong): Add insets for the tri_view of the toggle row.
  toggle_row->SetExpandable(true);
}

void FocusModeDetailedView::OnDoNotDisturbToggleClicked() {
  auto* controller = FocusModeController::Get();
  const bool is_on = do_not_disturb_toggle_button_->GetIsOn();
  if (controller->in_focus_session()) {
    message_center::MessageCenter::Get()->SetQuietMode(is_on);
  } else {
    controller->set_turn_on_do_not_disturb(is_on);
  }
}

BEGIN_METADATA(FocusModeDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
