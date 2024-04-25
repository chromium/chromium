// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_toast_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

void ConfigureLabel(views::Label* label, SkColor color, int font_size) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(color);

  gfx::Font default_font;
  gfx::Font label_font =
      default_font.Derive(font_size - default_font.GetFontSize(),
                          gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
}

}  // namespace

// View shown if the privacy screen setting is enterprise managed.
class PrivacyScreenToastManagedView : public views::View {
  METADATA_HEADER(PrivacyScreenToastManagedView, views::View)

 public:
  PrivacyScreenToastManagedView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kUnifiedManagedDeviceSpacing));

    views::Label* label = new views::Label();
    views::ImageView* icon = new views::ImageView();

    const AshColorProvider* color_provider = AshColorProvider::Get();
    const SkColor label_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary);
    ConfigureLabel(label, label_color, kPrivacyScreenToastSubLabelFontSize);

    label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ENTERPRISE_MANAGED));

    icon->SetPreferredSize(
        gfx::Size(kUnifiedSystemInfoHeight, kUnifiedSystemInfoHeight));

    const SkColor icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary);
    icon->SetImage(gfx::CreateVectorIcon(kSystemTrayManagedIcon, icon_color));

    AddChildView(label);
    AddChildView(icon);
  }

  ~PrivacyScreenToastManagedView() override = default;
};

BEGIN_METADATA(PrivacyScreenToastManagedView)
END_METADATA

// View containing the various labels in the toast.
class PrivacyScreenToastLabelView : public views::View {
  METADATA_HEADER(PrivacyScreenToastLabelView, views::View)

 public:
  PrivacyScreenToastLabelView() {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    label_ = new views::Label();
    managed_view_ = new PrivacyScreenToastManagedView();
    AddChildView(label_.get());
    AddChildView(managed_view_.get());

    const AshColorProvider* color_provider = AshColorProvider::Get();
    const SkColor primary_text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);

    ConfigureLabel(label_, primary_text_color,
                   kPrivacyScreenToastMainLabelFontSize);
  }

  ~PrivacyScreenToastLabelView() override = default;

  void SetPrivacyScreenEnabled(bool enabled, bool managed) {
    if (enabled) {
      label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE));
    } else {
      label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE));
    }

    managed_view_->SetVisible(managed);
  }

 private:
  raw_ptr<views::Label> label_;
  raw_ptr<PrivacyScreenToastManagedView> managed_view_;
};

BEGIN_METADATA(PrivacyScreenToastLabelView)
END_METADATA

PrivacyScreenToastView::PrivacyScreenToastView(
    PrivacyScreenToastController* controller,
    views::Button::PressedCallback callback)
    : controller_(controller) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kPrivacyScreenToastInsets,
      kPrivacyScreenToastSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  button_ =
      new FeaturePodIconButton(std::move(callback), /*is_togglable=*/true);
  button_->SetVectorIcon(kPrivacyScreenIcon);
  button_->SetToggled(false);
  button_->AddObserver(this);
  AddChildView(button_.get());

  label_ = new PrivacyScreenToastLabelView();
  AddChildView(label_.get());
}

PrivacyScreenToastView::~PrivacyScreenToastView() {
  button_->RemoveObserver(this);
}

void PrivacyScreenToastView::SetPrivacyScreenEnabled(bool enabled,
                                                     bool managed) {
  is_enabled_ = enabled;
  is_managed_ = managed;
  button_->SetToggled(enabled);
  label_->SetPrivacyScreenEnabled(enabled, managed);

  std::u16string enabled_state = l10n_util::GetStringUTF16(
      is_enabled_ ? IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE
                  : IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE);
  std::u16string managed_state =
      is_managed_ ? l10n_util::GetStringUTF16(
                        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ENTERPRISE_MANAGED)
                  : std::u16string();
  button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOOLTIP, enabled_state));

  accessible_name_ = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOAST_ACCESSIBILITY_TEXT,
      enabled_state, managed_state);

  DeprecatedLayoutImmediately();
}

bool PrivacyScreenToastView::IsButtonFocused() const {
  return button_->HasFocus();
}

void PrivacyScreenToastView::OnViewFocused(views::View* observed_view) {
  DCHECK(observed_view == button_);
  controller_->StopAutocloseTimer();
}

void PrivacyScreenToastView::OnViewBlurred(views::View* observed_view) {
  DCHECK(observed_view == button_);
  controller_->StartAutoCloseTimer();
}

BEGIN_METADATA(PrivacyScreenToastView)
END_METADATA

}  // namespace ash
