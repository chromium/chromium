// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/public_account_warning_dialog.h"

#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDialogWidthDp = 400;
constexpr int kDialogHeightDp = 160;
constexpr int kDialogContentMarginDp = 13;

constexpr int kBulletRadiusDp = 2;
constexpr int kBulletContainerSizeDp = 22;

constexpr int kLineHeightDp = 15;
constexpr int kBetweenLabelPaddingDp = 4;

class BulletView : public views::View {
 public:
  explicit BulletView(SkColor color, int radius)
      : color_(color), radius_(radius) {}

  ~BulletView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);

    SkPath path;
    path.addCircle(GetLocalBounds().CenterPoint().x(),
                   GetLocalBounds().CenterPoint().y(), radius_);
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
    flags.setColor(color_);
    flags.setAntiAlias(true);

    canvas->DrawPath(path, flags);
  }

 private:
  SkColor color_;
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(BulletView);
};

}  // namespace

PublicAccountWarningDialog::PublicAccountWarningDialog(
    base::WeakPtr<LoginExpandedPublicAccountView> controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kBetweenLabelPaddingDp));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kDialogContentMarginDp)));

  auto add_bulleted_label = [&](const base::string16& text) {
    auto* container = new views::View();
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    auto* label = new views::Label(text);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetLineHeight(kLineHeightDp);
    label->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    label->SetEnabledColor(SK_ColorGRAY);

    auto* bullet_view =
        new BulletView(label->GetEnabledColor(), kBulletRadiusDp);
    bullet_view->SetPreferredSize(
        gfx::Size(kBulletContainerSizeDp, kBulletContainerSizeDp));

    container->AddChildView(bullet_view);
    container->AddChildView(label);
    AddChildView(container);
  };

  add_bulleted_label(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_1));
  add_bulleted_label(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_2));
  add_bulleted_label(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_3));
  add_bulleted_label(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_4));

  // Widget will take the owership of this view.
  views::DialogDelegate::CreateDialogWidget(
      this, nullptr, controller->GetWidget()->GetNativeView());
}

PublicAccountWarningDialog::~PublicAccountWarningDialog() {
  if (controller_)
    controller_->OnWarningDialogClosed();
}

bool PublicAccountWarningDialog::IsVisible() {
  return GetWidget() && GetWidget()->IsVisible();
}

void PublicAccountWarningDialog::Show() {
  if (GetWidget())
    GetWidget()->Show();
}

int PublicAccountWarningDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void PublicAccountWarningDialog::AddedToWidget() {
  std::unique_ptr<views::Label> title_label =
      views::BubbleFrameView::CreateDefaultTitleLabel(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO));
  title_label->SetFontList(title_label->font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  auto* frame_view = static_cast<views::BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
  frame_view->SetTitleView(std::move(title_label));
}

ui::ModalType PublicAccountWarningDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

gfx::Size PublicAccountWarningDialog::CalculatePreferredSize() const {
  return gfx::Size(kDialogWidthDp, kDialogHeightDp);
}

}  // namespace ash
