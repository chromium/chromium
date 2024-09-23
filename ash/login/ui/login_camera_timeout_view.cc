// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_camera_timeout_view.h"

#include <utility>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/style/ash_color_id.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr char kLoginCameraTimeoutViewClassName[] = "LoginCameraTimeoutView";
constexpr char kLoginCameraTimeoutViewTextContainer[] = "TextContainer";

// Arrow button size.
constexpr int kArrowButtonSizeDp = 48;

// Font size of the title.
constexpr int kFontDeltaTitle = 12;

// Font size of the subtitle.
constexpr int kFontDeltaSubtitle = 2;

// Text vertical spacing.
constexpr int kTextVerticalSpacing = 10;

// Vertical spacing between text and arrow button.
constexpr int kLoginCameraTimeoutViewVerticalSpacing = 32;

views::Label* CreateLabel(const std::u16string& text, const int font_delta) {
  views::Label* label = new views::Label(
      text,
      {views::Label::GetDefaultFontList().DeriveWithSizeDelta(font_delta)});
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColorId(kColorAshTextColorPrimary);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

}  // namespace

LoginCameraTimeoutView::TestApi::TestApi(LoginCameraTimeoutView* view)
    : view_(view) {}

LoginCameraTimeoutView::TestApi::~TestApi() = default;

views::View* LoginCameraTimeoutView::TestApi::arrow_button() const {
  return view_->arrow_button_;
}

LoginCameraTimeoutView::LoginCameraTimeoutView(
    views::Button::PressedCallback callback)
    : NonAccessibleView(kLoginCameraTimeoutViewClassName) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kLoginCameraTimeoutViewVerticalSpacing));

  // Add text.
  views::View* text_container =
      new NonAccessibleView(kLoginCameraTimeoutViewTextContainer);
  auto* text_container_layout =
      text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kTextVerticalSpacing));
  text_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  text_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  AddChildView(text_container);
  // TODO(dkuzmin): Make title in Google Sans font once
  // https://crbug.com/1288022 is resolved.
  title_ = text_container->AddChildView(CreateLabel(
      l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_CAMERA_TIME_OUT_TITLE,
                                 ui::GetChromeOSDeviceName()),
      kFontDeltaTitle));
  subtitle_ = text_container->AddChildView(CreateLabel(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_CAMERA_TIME_OUT_SUBTITLE),
      kFontDeltaSubtitle));

  // Create arrow button.
  auto arrow_button = std::make_unique<ArrowButtonView>(std::move(callback),
                                                        kArrowButtonSizeDp);
  arrow_button->GetViewAccessibility().SetName(base::JoinString(
      {l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_CAMERA_TIME_OUT_TITLE,
                                  ui::GetChromeOSDeviceName()),
       l10n_util::GetStringUTF16(IDS_ASH_LOGIN_CAMERA_TIME_OUT_SUBTITLE)},
      u" "));
  arrow_button->SetFocusPainter(nullptr);

  // Arrow button size should be its preferred size so we wrap it.
  auto* arrow_button_container =
      AddChildView(std::make_unique<NonAccessibleView>());
  auto container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  arrow_button_container->SetLayoutManager(std::move(container_layout));
  arrow_button_ = arrow_button_container->AddChildView(std::move(arrow_button));
}

LoginCameraTimeoutView::~LoginCameraTimeoutView() = default;

void LoginCameraTimeoutView::RequestFocus() {
  return arrow_button_->RequestFocus();
}

BEGIN_METADATA(LoginCameraTimeoutView)
END_METADATA

}  // namespace ash
