// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/passpoint_dialog_view.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/net/browser_url_opener.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
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
// Bottom margin for the Passpoint dialog's body label.
constexpr int kDialogBodyBottomMargin = 23;
// Expiration time for showing Passpoint additional dialog.
constexpr base::TimeDelta kDialogExpiration = base::Days(365);

// |subscription_expiration_time_ms| is in the format of number of milliseconds
// since January 1, 1970, 00:00:00 GMT. Expiration time of int64_min means no
// expiry date based on Android's behavior.
std::optional<base::Time> GetTimeFromSubscriptionExpirationMs(
    int64_t subscription_expiration_time_ms) {
  if (subscription_expiration_time_ms == std::numeric_limits<int64_t>::min()) {
    return std::nullopt;
  }
  return base::Time::UnixEpoch() +
         base::Milliseconds(subscription_expiration_time_ms);
}

// Returns true if the expiration time is within |kDialogExpiration| and is
// still valid.
bool IsExpiring(std::optional<base::Time> subscription_expiration_time) {
  if (!subscription_expiration_time.has_value()) {
    return false;
  }
  auto expiry_time_delta =
      subscription_expiration_time.value() - base::Time::Now();
  return expiry_time_delta.is_positive() &&
         (expiry_time_delta < kDialogExpiration);
}

}  // namespace

namespace arc {

PasspointDialogView::PasspointDialogView(
    mojom::PasspointApprovalRequestPtr request,
    PasspointDialogCallback callback)
    : app_name_(base::UTF8ToUTF16(request->app_name)),
      callback_(std::move(callback)) {
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
              IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_TITLE, app_name_))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAllowCharacterBreak(true)
          .SetFontList(
              views::TypographyProvider::Get()
                  .GetFont(views::style::TextContext::CONTEXT_DIALOG_TITLE,
                           views::style::TextStyle::STYLE_PRIMARY)
                  .DeriveWithWeight(gfx::Font::Weight::MEDIUM))
          .Build());

  AddChildView(
      MakeContentsView(IsExpiring(GetTimeFromSubscriptionExpirationMs(
                           request->subscription_expiration_time_ms)),
                       request->friendly_name.value_or(std::string())));
  AddChildView(MakeButtonsView());
}

PasspointDialogView::~PasspointDialogView() = default;

gfx::Size PasspointDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

void PasspointDialogView::AddedToWidget() {
  auto& view_ax = GetWidget()->GetRootView()->GetViewAccessibility();
  view_ax.SetRole(ax::mojom::Role::kDialog);
  view_ax.SetName(l10n_util::GetStringFUTF16(
                      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_TITLE, app_name_),
                  ax::mojom::NameFrom::kAttribute);
}

std::unique_ptr<views::View> PasspointDialogView::MakeBaseLabelView(
    bool is_expiring) {
  const std::u16string label = l10n_util::GetStringFUTF16(
      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_BODY, ui::GetChromeOSDeviceName());

  std::unique_ptr<views::StyledLabel> styled_label =
      views::Builder<views::StyledLabel>()
          .CopyAddressTo(&body_text_)
          .SetText(label)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAutoColorReadabilityEnabled(false)
          .Build();

  if (!is_expiring) {
    std::vector<size_t> offsets;
    const std::u16string learn_more = l10n_util::GetStringUTF16(
        IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_LEARN_MORE_LABEL);
    const std::u16string label_expiring = l10n_util::GetStringFUTF16(
        IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_BODY_WITH_LEARN_MORE,
        ui::GetChromeOSDeviceName(), learn_more, &offsets);
    styled_label->SetText(label_expiring);
    styled_label->AddStyleRange(
        gfx::Range(offsets.back(), offsets.back() + learn_more.length()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&PasspointDialogView::OnLearnMoreClicked,
                                weak_factory_.GetWeakPtr())));
  }
  return styled_label;
}

std::unique_ptr<views::View> PasspointDialogView::MakeSubscriptionLabelView(
    std::string_view friendly_name) {
  std::vector<size_t> offsets;
  const std::u16string learn_more = l10n_util::GetStringUTF16(
      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_LEARN_MORE_LABEL);
  const std::u16string label = l10n_util::GetStringFUTF16(
      IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_SUBSCRIPTION_BODY_WITH_LEARN_MORE,
      {ui::GetChromeOSDeviceName(), base::UTF8ToUTF16(friendly_name),
       learn_more},
      &offsets);
  std::unique_ptr<views::StyledLabel> styled_label =
      views::Builder<views::StyledLabel>()
          .CopyAddressTo(&body_subscription_text_)
          .SetText(label)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAutoColorReadabilityEnabled(false)
          .AddStyleRange(
              gfx::Range(offsets.back(), offsets.back() + learn_more.length()),
              views::StyledLabel::RangeStyleInfo::CreateForLink(
                  base::BindRepeating(&PasspointDialogView::OnLearnMoreClicked,
                                      weak_factory_.GetWeakPtr())))
          .Build();
  return styled_label;
}

std::unique_ptr<views::View> PasspointDialogView::MakeContentsView(
    bool is_expiring,
    std::string_view friendly_name) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  std::unique_ptr<views::BoxLayoutView> contents =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, 0, kDialogBodyBottomMargin, 0))
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL))
          .Build();

  contents->AddChildView(MakeBaseLabelView(is_expiring));
  if (is_expiring) {
    contents->AddChildView(MakeSubscriptionLabelView(friendly_name));
  }
  return contents;
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
              .SetStyle(ui::ButtonStyle::kDefault)
              .SetIsDefault(false),
          views::Builder<views::MdTextButton>()  // Allow button.
              .CopyAddressTo(&allow_button_)
              .SetCallback(base::BindRepeating(
                  &PasspointDialogView::OnButtonClicked,
                  weak_factory_.GetWeakPtr(), /*allow=*/true))
              .SetText(l10n_util::GetStringUTF16(
                  IDS_ASH_ARC_PASSPOINT_APP_APPROVAL_ALLOW_BUTTON))
              .SetStyle(ui::ButtonStyle::kProminent)
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
                               mojom::PasspointApprovalRequestPtr request,
                               PasspointDialogCallback callback) {
  // This is safe because the callback is triggered only on button click which
  // requires the dialog to be valid. The dialog is deleted alongside the
  // parent's destructor.
  auto remove_overlay =
      base::BindOnce(&OverlayDialog::CloseIfAny, base::Unretained(parent));

  auto dialog_view = std::make_unique<PasspointDialogView>(
      std::move(request), std::move(callback).Then(std::move(remove_overlay)));
  auto* dialog_view_ptr = dialog_view.get();

  OverlayDialog::Show(
      parent,
      base::BindOnce(&PasspointDialogView::OnButtonClicked,
                     base::Unretained(dialog_view_ptr), /*allow=*/false),
      std::move(dialog_view));
}

}  // namespace arc
