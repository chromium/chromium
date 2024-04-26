// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_toast_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_toast_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace ash {

AutozoomToastView::AutozoomToastView(AutozoomToastController* controller)
    : controller_(controller) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kAutozoomToastInsets,
      kAutozoomToastSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  button_ = AddChildView(std::make_unique<FeaturePodIconButton>(
      FeaturePodIconButton::PressedCallback(),
      /*is_togglable=*/true));
  button_->SetVectorIcon(kUnifiedMenuAutozoomIcon);
  button_->SetToggled(false);
  button_->AddObserver(this);

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_TOAST_ON_STATE));
  label_->SetFontList(views::TypographyProvider::Get().GetFont(
      views::style::TextContext::CONTEXT_DIALOG_TITLE,
      views::style::TextStyle::STYLE_PRIMARY));
}

AutozoomToastView::~AutozoomToastView() {
  button_->RemoveObserver(this);
}

void AutozoomToastView::SetAutozoomEnabled(bool enabled) {
  button_->SetToggled(enabled);
  button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_TOAST_ON_STATE));

  InvalidateLayout();
}

bool AutozoomToastView::IsButtonFocused() const {
  return button_->HasFocus();
}

std::u16string AutozoomToastView::accessible_name() const {
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_TOAST_ON_STATE);
}

void AutozoomToastView::OnViewFocused(views::View* observed_view) {
  DCHECK(observed_view == button_);
  controller_->StopAutocloseTimer();
}

void AutozoomToastView::OnViewBlurred(views::View* observed_view) {
  DCHECK(observed_view == button_);
  controller_->StartAutoCloseTimer();
}

BEGIN_METADATA(AutozoomToastView)
END_METADATA

}  // namespace ash
