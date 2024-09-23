// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/public_account_monitoring_info_dialog.h"

#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDialogWidthDp = 400;
constexpr int kDialogContentMarginDp = 13;

constexpr int kBulletRadiusDp = 2;
constexpr int kBulletContainerSizeDp = 22;

constexpr int kLabelMaximumWidth =
    kDialogWidthDp - kBulletContainerSizeDp - 2 * kDialogContentMarginDp;

class BulletView : public views::View {
  METADATA_HEADER(BulletView, views::View)

 public:
  explicit BulletView(SkColor color, int radius)
      : color_(color), radius_(radius) {}
  BulletView(const BulletView&) = delete;
  BulletView& operator=(const BulletView&) = delete;
  ~BulletView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);

    SkPath path;
    path.addCircle(GetLocalBounds().CenterPoint().x(),
                   GetLocalBounds().CenterPoint().y(), radius_);
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color_);
    flags.setAntiAlias(true);

    canvas->DrawPath(path, flags);
  }

 private:
  SkColor color_;
  int radius_;
};

BEGIN_METADATA(BulletView)
END_METADATA

}  // namespace

PublicAccountMonitoringInfoDialog::PublicAccountMonitoringInfoDialog(
    base::WeakPtr<LoginExpandedPublicAccountView> controller)
    : controller_(controller) {
  SetModalType(ui::mojom::ModalType::kSystem);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  SetLayoutManager(std::move(layout));
  SetBorder(views::CreateEmptyBorder(kDialogContentMarginDp));

  auto add_bulleted_label = [&](const std::u16string& text) {
    auto* container = new views::View();
    auto layout = std::make_unique<views::FlexLayout>();
    // Align the bullet to the top line of multi-line labels.
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    container->SetLayoutManager(std::move(layout));
    auto* label = new views::Label(text, views::style::CONTEXT_LABEL);
    // If text style is set in the constructor, colors will not be updated and
    // GetEnabledColor will return default color. See crbug.com/1151261.
    label->SetTextStyle(views::style::STYLE_SECONDARY);
    label->SetMultiLine(true);
    label->SetMaximumWidth(kLabelMaximumWidth);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

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

PublicAccountMonitoringInfoDialog::~PublicAccountMonitoringInfoDialog() {
  if (controller_) {
    controller_->OnLearnMoreDialogClosed();
  }
}

bool PublicAccountMonitoringInfoDialog::IsVisible() {
  return GetWidget() && GetWidget()->IsVisible();
}

void PublicAccountMonitoringInfoDialog::Show() {
  if (GetWidget()) {
    GetWidget()->Show();
  }
}

void PublicAccountMonitoringInfoDialog::AddedToWidget() {
  std::unique_ptr<views::Label> title_label =
      views::BubbleFrameView::CreateDefaultTitleLabel(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO));
  title_label->SetFontList(title_label->font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  auto* frame_view = static_cast<views::BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
  frame_view->SetTitleView(std::move(title_label));
}

gfx::Size PublicAccountMonitoringInfoDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return {kDialogWidthDp,
          GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidthDp)};
}

BEGIN_METADATA(PublicAccountMonitoringInfoDialog)
END_METADATA

}  // namespace ash
