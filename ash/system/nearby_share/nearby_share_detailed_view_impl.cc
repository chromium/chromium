// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr auto kToggleRowTriViewInsets = gfx::Insets::VH(8, 24);
constexpr auto kVisibilitySelectionContainerMargins =
    gfx::Insets::TLBR(2, 0, 0, 0);
}  // namespace

namespace ash {

NearbyShareDetailedViewImpl::NearbyShareDetailedViewImpl(
    DetailedViewDelegate* detailed_view_delegate)
    : TrayDetailedView(detailed_view_delegate),
      nearby_share_delegate_(Shell::Get()->nearby_share_delegate()) {
  // TODO(brandosocarras, b/360150790): Create and use a Quick Share string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL);
  CreateScrollableList();
  CreateIsEnabledContainer();
  CreateVisibilitySelectionContainer();
}

NearbyShareDetailedViewImpl::~NearbyShareDetailedViewImpl() = default;

views::View* NearbyShareDetailedViewImpl::GetAsView() {
  return this;
}

void NearbyShareDetailedViewImpl::CreateExtraTitleRowButtons() {
  DCHECK(!settings_button_);

  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  settings_button_ = CreateSettingsButton(
      base::BindRepeating(&NearbyShareDetailedViewImpl::OnSettingsButtonClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_STATUS_TRAY_NEARBY_SHARE_BUTTON_LABEL);
  settings_button_->SetState(TrayPopupUtils::CanOpenWebUISettings()
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void NearbyShareDetailedViewImpl::HandleViewClicked(views::View* view) {}

void NearbyShareDetailedViewImpl::CreateIsEnabledContainer() {
  DCHECK(!is_enabled_container_);
  DCHECK(scroll_content());
  DCHECK(!toggle_row_);

  is_enabled_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));
  is_enabled_container_->SetBorderInsets(gfx::Insets());
  toggle_row_ = is_enabled_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row_->SetFocusBehavior(FocusBehavior::NEVER);

  // TODO(brandosocarras, b/360150790): Create and use a 'Who can share with
  // you' string.
  toggle_row_->AddLabelRow(u"Who can share with you", /*start_inset=*/0);
  toggle_row_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *toggle_row_->text_label());

  auto toggle_switch = std::make_unique<Switch>(base::BindRepeating(
      &NearbyShareDetailedViewImpl::OnQuickShareToggleClicked,
      weak_factory_.GetWeakPtr()));
  toggle_switch_ = toggle_switch.get();
  toggle_row_->AddRightView(toggle_switch.release());

  // Allow the row to be taller than a typical tray menu item.
  toggle_row_->SetExpandable(true);
  toggle_row_->tri_view()->SetInsets(kToggleRowTriViewInsets);

  // ChromeVox users will just use the toggle switch to toggle.
  toggle_row_->text_label()->GetViewAccessibility().SetIsIgnored(true);
}

void NearbyShareDetailedViewImpl::CreateVisibilitySelectionContainer() {
  DCHECK(!visibility_selection_container_);
  DCHECK(scroll_content());

  visibility_selection_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));

  // Add a small empty space, like a separator, between the containers.
  visibility_selection_container_->SetProperty(
      views::kMarginsKey, kVisibilitySelectionContainerMargins);

  CreateYourDevicesRow();
  CreateContactsRow();
  CreateEveryoneRow();
  CreateOnlyForTenMinutesRow();
}

void NearbyShareDetailedViewImpl::CreateYourDevicesRow() {
  DCHECK(!your_devices_row_);
  DCHECK(visibility_selection_container_);

  your_devices_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));

  // TODO(brandosocarras, b/360150790): replace label, sublabel with IDS
  // strings.
  CreateVisibilityRow(your_devices_row_,
                      kQuickSettingsQuickShareYourDevicesIcon,
                      /*label=*/u"Your devices",
                      /*sublabel=*/u"Only devices signed into test@gmail.com");
}

void NearbyShareDetailedViewImpl::CreateContactsRow() {
  DCHECK(!contacts_row_);
  DCHECK(visibility_selection_container_);

  contacts_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));

  // TODO(brandosocarras, b/360150790): replace label, sublabel with IDS
  // strings.
  CreateVisibilityRow(contacts_row_, kQuickSettingsQuickShareContactsIcon,
                      /*label=*/u"Contacts",
                      /*sublabel=*/u"Only your contacts with a Google Account");
}

void NearbyShareDetailedViewImpl::CreateEveryoneRow() {
  DCHECK(!everyone_row_);
  DCHECK(visibility_selection_container_);

  everyone_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));

  // TODO(brandosocarras, b/360150790): replace label, sublabel with IDS
  // strings.
  CreateVisibilityRow(everyone_row_, kQuickSettingsQuickShareEveryoneIcon,
                      /*label=*/u"Everyone", /*sublabel=*/u"Anyone nearby");
}

void NearbyShareDetailedViewImpl::CreateVisibilityRow(
    HoverHighlightView* visibility_row,
    const gfx::VectorIcon& vector_icon,
    const std::u16string& label,
    const std::u16string& sublabel) {
  DCHECK(visibility_row);
  visibility_row->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(
          vector_icon, /*color_id=*/cros_tokens::kCrosSysOnSurface),
      label);
  visibility_row->SetSubText(sublabel);
}

void NearbyShareDetailedViewImpl::CreateOnlyForTenMinutesRow() {
  DCHECK(!only_for_ten_minutes_row_);
  DCHECK(visibility_selection_container_);

  only_for_ten_minutes_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  only_for_ten_minutes_row_->SetFocusBehavior(FocusBehavior::NEVER);
  only_for_ten_minutes_row_->AddLabelRow(u"Only for 10 minutes",
                                         /*start_inset=*/20);
  only_for_ten_minutes_row_->text_label()->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosBody2,
      *only_for_ten_minutes_row_->text_label());

  auto toggle_switch = std::make_unique<Switch>(base::BindRepeating(
      &NearbyShareDetailedViewImpl::OnTenMinutesToggleClicked,
      weak_factory_.GetWeakPtr()));
  only_for_ten_minutes_row_->AddRightView(toggle_switch.release());

  // ChromeVox users will just use the toggle switch to toggle.
  only_for_ten_minutes_row_->text_label()->GetViewAccessibility().SetIsIgnored(
      true);
}

void NearbyShareDetailedViewImpl::OnSettingsButtonClicked() {
  CloseBubble();
  Shell::Get()->system_tray_model()->client()->ShowNearbyShareSettings();
}

// TODO(brandosocarras, b/360150790): Implement this when this class is able to
// set the device's QSv2 quick share visibility. This occurs when 1) this class
// can set the device's nearby share visibility and 2) when, while QSv2 is
// enabled, the device can remember previously set visibility and high
// visibility settings in the event QS is switched off.
void NearbyShareDetailedViewImpl::OnQuickShareToggleClicked() {}

void NearbyShareDetailedViewImpl::OnYourDevicesSelected() {}

void NearbyShareDetailedViewImpl::OnContactsSelected() {}

void NearbyShareDetailedViewImpl::OnEveryoneSelected() {}

void NearbyShareDetailedViewImpl::OnTenMinutesToggleClicked() {}

BEGIN_METADATA(NearbyShareDetailedViewImpl)
END_METADATA

}  // namespace ash
