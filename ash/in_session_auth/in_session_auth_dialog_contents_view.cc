// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_contents_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"
#include "components/account_id/account_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

constexpr int kTopMargin = 5;
constexpr int kRightMargin = 5;
constexpr int kBottomMargin = 20;

constexpr int kCornerRadius = 12;
constexpr int kPreferredWidth = 340;
constexpr int kPromptLineHeight = 18;
constexpr int kTitleFontSizeDeltaDp = 4;

constexpr int kAvatarSizeDp = 36;

constexpr int kSpacingAfterAvatar = 18;
constexpr int kSpacingAfterTitle = 8;
constexpr int kSpacingAfterPrompt = 32;

UserAvatar GetActiveUserAvatar() {
  Shell* shell = Shell::Get();
  AccountId account_id = shell->session_controller()->GetActiveAccountId();
  const UserSession* session =
      shell->session_controller()->GetUserSessionByAccountId(account_id);
  DCHECK(session);
  return session->user_info.avatar;
}

}  // namespace

InSessionAuthDialogContentsView::InSessionAuthDialogContentsView(
    const std::optional<std::string>& prompt,
    base::OnceClosure on_end_authentication,
    base::RepeatingClosure on_ui_initialized,
    AuthHubConnector* connector) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true);

  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW,
      ui::kColorPrimaryBackground);
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  AddVerticalSpacing(kTopMargin);
  AddCloseButton();
  AddUserAvatar();
  AddVerticalSpacing(kSpacingAfterAvatar);
  AddTitle();
  AddVerticalSpacing(kSpacingAfterTitle);

  if (prompt.has_value()) {
    AddPrompt(prompt.value());
    AddVerticalSpacing(kSpacingAfterPrompt);
  }

  AddAuthPanel(std::move(on_end_authentication), std::move(on_ui_initialized),
               connector);

  AddVerticalSpacing(kBottomMargin);
}

InSessionAuthDialogContentsView::~InSessionAuthDialogContentsView() = default;

void InSessionAuthDialogContentsView::AddVerticalSpacing(int height) {
  auto* spacing = AddChildView(std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kPreferredWidth, height));
}

void InSessionAuthDialogContentsView::AddUserAvatar() {
  avatar_view_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
      gfx::Size(kAvatarSizeDp, kAvatarSizeDp),
      /*corner_radius=*/kAvatarSizeDp / 2));

  UserAvatar avatar = GetActiveUserAvatar();

  avatar_view_->SetImage(avatar.image);
}

void InSessionAuthDialogContentsView::AddCloseButton() {
  close_button_container_ = AddChildView(std::make_unique<NonAccessibleView>());

  close_button_container_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true);

  auto callback = [](const ui::Event& event) {
    // TODO(b/271248452): placeholder.
    LOG(ERROR) << "pressed";
  };

  std::unique_ptr<views::ImageButton> close_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(callback), views::kIcCloseIcon);

  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SizeToPreferredSize();
  close_button->SetVisible(true);
  close_button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  views::InstallCircleHighlightPathGenerator(close_button.get());

  close_button_ =
      close_button_container_->AddChildView(std::move(close_button));

  auto* spacing = close_button_container_->AddChildView(
      std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kRightMargin, close_button_->height()));

  close_button_container_->SetPreferredSize(
      gfx::Size{kPreferredWidth,
                close_button_container_->GetHeightForWidth(kPreferredWidth)});
}

void InSessionAuthDialogContentsView::AddTitle() {
  title_ = AddChildView(std::make_unique<views::Label>());

  title_->SetSubpixelRenderingEnabled(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  title_->SetFontList(base_font_list.Derive(kTitleFontSizeDeltaDp,
                                            gfx::Font::FontStyle::NORMAL,
                                            gfx::Font::Weight::MEDIUM));
  title_->SetMaximumWidthSingleLine(kPreferredWidth);
  title_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  title_->SetPreferredSize(
      gfx::Size(kPreferredWidth, title_->GetHeightForWidth(kPreferredWidth)));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  std::u16string title_text =
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE);
  title_->SetText(title_text);
  title_->SetEnabledColorId(kColorAshTextColorPrimary);
  title_->SetAccessibleName(title_text);
}

void InSessionAuthDialogContentsView::AddPrompt(const std::string& prompt) {
  prompt_view_ = AddChildView(std::make_unique<views::Label>());
  prompt_view_->SetEnabledColorId(kColorAshTextColorSecondary);
  prompt_view_->SetSubpixelRenderingEnabled(false);
  prompt_view_->SetAutoColorReadabilityEnabled(false);
  prompt_view_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  prompt_view_->SetText(base::UTF8ToUTF16(prompt));
  prompt_view_->SetMultiLine(true);
  prompt_view_->SetMaximumWidth(kPreferredWidth);
  prompt_view_->SetLineHeight(kPromptLineHeight);

  prompt_view_->SetPreferredSize(gfx::Size(
      kPreferredWidth, prompt_view_->GetHeightForWidth(kPreferredWidth)));
  prompt_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

void InSessionAuthDialogContentsView::AddAuthPanel(
    base::OnceClosure on_end_authentication,
    base::RepeatingClosure on_ui_initialized,
    AuthHubConnector* connector) {
  auth_panel_ = AddChildView(std::make_unique<AuthPanel>(
      std::make_unique<FactorAuthViewFactory>(),
      std::make_unique<AuthFactorStoreFactory>(),
      std::make_unique<AuthPanelEventDispatcherFactory>(),
      std::move(on_end_authentication), std::move(on_ui_initialized),
      connector));
}

AuthPanel* InSessionAuthDialogContentsView::GetAuthPanel() {
  return auth_panel_;
}

BEGIN_METADATA(InSessionAuthDialogContentsView)
END_METADATA

}  // namespace ash
