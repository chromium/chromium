// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_header_view.h"

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/style/typography.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kIconToTitleDistanceDp = 16;
constexpr int kAvatarSizeDp = 40;

constexpr ui::ColorId kTitleColorId = cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kTitleErrorColorId = cros_tokens::kCrosSysError;
// The title and description width is the kActiveSessionAuthViewWidthDp -
// 2 X 32 dp margin.
constexpr int kTitleLineWidthDp = 322 - 2 * 32;
constexpr int kTitleToDescriptionDistanceDp = 8;
constexpr TypographyToken kTitleFont = TypographyToken::kCrosTitle1;

constexpr ui::ColorId kDescriptionColorId = cros_tokens::kCrosSysOnSurface;
constexpr int kDescriptionLineWidthDp = kTitleLineWidthDp;
constexpr TypographyToken kDescriptionFont = TypographyToken::kCrosAnnotation1;

}  // namespace

AuthHeaderView::TestApi::TestApi(AuthHeaderView* view) : view_(view) {}
AuthHeaderView::TestApi::~TestApi() = default;

AuthHeaderView::Observer::Observer() = default;
AuthHeaderView::Observer::~Observer() = default;

const std::u16string& AuthHeaderView::TestApi::GetCurrentTitle() const {
  return view_->title_label_->GetText();
}

raw_ptr<AuthHeaderView> AuthHeaderView::TestApi::GetView() {
  return view_;
}

AuthHeaderView::AuthHeaderView(const AccountId& account_id,
                               const std::u16string& title,
                               const std::u16string& description)
    : title_str_(title) {
  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  };

  auto add_spacer = [&](int height) {
    auto* spacer = new NonAccessibleView();
    spacer->SetPreferredSize(gfx::Size(0, height));
    AddChildView(spacer);
  };

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  // Add avatar.
  avatar_view_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
      gfx::Size(kAvatarSizeDp, kAvatarSizeDp),
      kAvatarSizeDp / 2 /*corner_radius*/));

  const UserAvatar avatar = BuildAshUserAvatarForAccountId(account_id);
  avatar_view_->SetImage(avatar.image);

  // Add vertical space separator.
  add_spacer(kIconToTitleDistanceDp);

  // Add title.
  title_label_ = new views::Label(title, views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  title_label_->SetMultiLine(true);
  title_label_->SizeToFit(kTitleLineWidthDp);
  title_label_->SetEnabledColorId(kTitleColorId);
  title_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(kTitleFont));
  title_label_->GetViewAccessibility().SetRole(ax::mojom::Role::kTitleBar);
  title_label_->GetViewAccessibility().SetName(title);
  decorate_label(title_label_);
  AddChildView(title_label_.get());

  // Add vertical space separator.
  add_spacer(kTitleToDescriptionDistanceDp);

  // Add description.
  description_label_ = new views::Label(
      description, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  description_label_->SetMultiLine(true);
  description_label_->SizeToFit(kDescriptionLineWidthDp);
  description_label_->SetEnabledColorId(kDescriptionColorId);
  description_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(kDescriptionFont));
  decorate_label(description_label_);
  AddChildView(description_label_.get());
}

AuthHeaderView::~AuthHeaderView() {
  avatar_view_ = nullptr;
  title_label_ = nullptr;
  description_label_ = nullptr;
}

gfx::Size AuthHeaderView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      kAvatarSizeDp + kIconToTitleDistanceDp +
      title_label_->GetHeightForWidth(kTitleLineWidthDp) +
      kTitleToDescriptionDistanceDp +
      description_label_->GetHeightForWidth(kDescriptionLineWidthDp);
  return gfx::Size(kTitleLineWidthDp, preferred_height);
}

void AuthHeaderView::SetErrorTitle(const std::u16string& error_str) {
  title_label_->SetText(error_str);
  title_label_->SetEnabledColorId(kTitleErrorColorId);
  NotifyTitleChanged(error_str);
  title_label_->GetViewAccessibility().SetName(error_str);
  title_label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                         /*send_native_event=*/true);
  title_label_->GetViewAccessibility().AnnounceText(error_str);
}

void AuthHeaderView::RestoreTitle() {
  if (title_label_->GetText() != title_str_) {
    title_label_->SetText(title_str_);
    title_label_->SetEnabledColorId(kTitleColorId);
    NotifyTitleChanged(title_str_);
  }
}

void AuthHeaderView::NotifyTitleChanged(const std::u16string& title) {
  for (auto& observer : observers_) {
    observer.OnTitleChanged(title);
  }
}

void AuthHeaderView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthHeaderView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(AuthHeaderView)
END_METADATA

}  // namespace ash
