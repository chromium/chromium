// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/passpoint_dialog_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/net/browser_url_opener.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "url/gurl.h"

namespace {

constexpr char kPasspointHelpPage[] =
    "https://support.google.com/chromebook?p=wifi_passpoint";

}  // namespace

namespace {

// Radius value to make the bubble border of the Passpoint dialog.
constexpr int kCornerRadius = 12;
// Top, left, bottom, and right inside margin for the Passpoint dialog.
constexpr int kDialogBorderMargin[] = {24, 24, 20, 24};
// Top, left, bottom, and right margin for the Passpoint dialog's body label.
constexpr int kDialogBodyMargin[] = {0, 0, 23, 0};

}  // namespace

namespace arc {

PasspointDialogView::PasspointDialogView(base::StringPiece app_name,
                                         PasspointDialogCallback callback)
    : callback_(std::move(callback)) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetInsideBorderInsets(
      gfx::Insets::TLBR(kDialogBorderMargin[0], kDialogBorderMargin[1],
                        kDialogBorderMargin[2], kDialogBorderMargin[3]));
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      ash::kColorAshDialogBackgroundColor);
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_TITLE,
              base::UTF8ToUTF16(app_name)))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAllowCharacterBreak(true)
          .SetFontList(views::style::GetFont(
                           views::style::TextContext::CONTEXT_DIALOG_TITLE,
                           views::style::TextStyle::STYLE_PRIMARY)
                           .DeriveWithWeight(gfx::Font::Weight::MEDIUM))
          .Build());

  AddChildView(MakeContentsView());
  AddChildView(MakeButtonsView());
}

PasspointDialogView::~PasspointDialogView() = default;

gfx::Size PasspointDialogView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  size.set_width(provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  return size;
}

std::unique_ptr<views::View> PasspointDialogView::MakeContentsView() {
  // Create the body text.
  std::vector<size_t> offsets;
  const std::u16string learn_more = l10n_util::GetStringUTF16(
      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_LEARN_MORE_LABEL);
  const std::u16string label = l10n_util::GetStringFUTF16(
      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_BODY, ui::GetChromeOSDeviceName(),
      learn_more, &offsets);

  // Create link style for "Learn more".
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&PasspointDialogView::OnLearnMoreClicked,
                              weak_factory_.GetWeakPtr()));

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(kDialogBodyMargin[0], kDialogBodyMargin[1],
                            kDialogBodyMargin[2], kDialogBodyMargin[3]))
      .AddChildren(
          views::Builder<views::StyledLabel>()
              .CopyAddressTo(&body_text_)
              .SetText(label)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .AddStyleRange(
                  gfx::Range(offsets[1], offsets[1] + learn_more.length()),
                  link_style)
              .SetAutoColorReadabilityEnabled(false))
      .Build();
}

std::unique_ptr<views::View> PasspointDialogView::MakeButtonsView() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
      .SetBetweenChildSpacing(provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddChildren(
          views::Builder<views::MdTextButton>()  // Don't allow button.
              .CopyAddressTo(&dont_allow_button_)
              .SetCallback(base::BindRepeating(
                  &PasspointDialogView::OnButtonClicked,
                  weak_factory_.GetWeakPtr(), /*allow=*/false))
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_DONT_ALLOW_BUTTON))
              .SetProminent(false)
              .SetIsDefault(false),
          views::Builder<views::MdTextButton>()  // Allow button.
              .CopyAddressTo(&allow_button_)
              .SetCallback(base::BindRepeating(
                  &PasspointDialogView::OnButtonClicked,
                  weak_factory_.GetWeakPtr(), /*allow=*/true))
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_ALLOW_BUTTON))
              .SetProminent(true)
              .SetIsDefault(true))
      .Build();
}

void PasspointDialogView::OnLearnMoreClicked() {
  BrowserUrlOpener::Get()->OpenUrl(GURL(kPasspointHelpPage));
}

void PasspointDialogView::OnButtonClicked(bool allow) {
  // Callback can be null as this is also called on dialog deletion.
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(mojom::PasspointApprovalResponse::New(allow));
}

void PasspointDialogView::Show(aura::Window* parent,
                               base::StringPiece app_name,
                               PasspointDialogCallback callback) {
  // This is safe because the callback is triggered only on button click which
  // requires the dialog to be valid. The dialog is deleted alongside the
  // parent's destructor.
  auto remove_overlay =
      base::BindOnce(&OverlayDialog::CloseIfAny, base::Unretained(parent));

  auto dialog_view = std::make_unique<PasspointDialogView>(
      app_name, std::move(callback).Then(std::move(remove_overlay)));
  auto* dialog_view_ptr = dialog_view.get();

  OverlayDialog::Show(
      parent,
      base::BindOnce(&PasspointDialogView::OnButtonClicked,
                     base::Unretained(dialog_view_ptr), /*allow=*/false),
      std::move(dialog_view));
}

}  // namespace arc
