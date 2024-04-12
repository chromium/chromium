// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_contents_view.h"

#include "ash/style/ash_color_id.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 12;
constexpr int kPreferredWidth = 340;
constexpr int kPromptLineHeight = 18;

constexpr int kSpacingAfterPrompt = 32;

}  // namespace

InSessionAuthDialogContentsView::InSessionAuthDialogContentsView(
    const std::optional<std::string>& prompt,
    base::OnceClosure on_end_authentication,
    base::RepeatingClosure on_ui_initialized,
    AuthHubConnector* connector) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(false);

  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW,
      ui::kColorPrimaryBackground);
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  if (prompt.has_value()) {
    AddPrompt(prompt.value());
    AddVerticalSpacing(kSpacingAfterPrompt);
  }

  AddAuthPanel(std::move(on_end_authentication), std::move(on_ui_initialized),
               connector);
}

InSessionAuthDialogContentsView::~InSessionAuthDialogContentsView() = default;

void InSessionAuthDialogContentsView::AddVerticalSpacing(int height) {
  auto* spacing = AddChildView(std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kPreferredWidth, height));
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
