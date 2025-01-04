// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

#include <string>

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

void FormatVisibilityRow(ash::HoverHighlightView* visibility_row,
                         const gfx::VectorIcon& vector_icon,
                         const std::u16string& label,
                         const std::u16string& sublabel,
                         const ui::ColorId color_id,
                         const bool is_row_enabled) {
  DCHECK(visibility_row);
  visibility_row->Reset();
  visibility_row->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(vector_icon, /*color_id=*/color_id),
      label);
  visibility_row->text_label()->SetEnabledColorId(color_id);
  visibility_row->SetSubText(sublabel);
  visibility_row->sub_text_label()->SetEnabledColorId(color_id);
  visibility_row->AddRightIcon(ui::ImageModel::FromVectorIcon(
                                   ash::kHollowCheckCircleIcon,
                                   /*color_id=*/cros_tokens::kCrosSysOnSurface),
                               20);
  visibility_row->SetRightViewVisible(false);
  visibility_row->SetEnabled(is_row_enabled);
}
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

void NearbyShareDetailedViewImpl::HandleViewClicked(views::View* view) {
  CHECK(your_devices_row_);
  CHECK(contacts_row_);

  if (view == your_devices_row_) {
    OnYourDevicesSelected();
    return;
  }

  if (view == contacts_row_) {
    OnContactsSelected();
    return;
  }

  if (view == hidden_row_) {
    OnHiddenSelected();
    return;
  }
}

void NearbyShareDetailedViewImpl::CreateIsEnabledContainer() {
  DCHECK(!is_enabled_container_);
  DCHECK(scroll_content());
  DCHECK(!toggle_row_);
  DCHECK(nearby_share_delegate_);

  is_enabled_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));
  is_enabled_container_->SetBorderInsets(gfx::Insets());
  toggle_row_ = is_enabled_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row_->SetFocusBehavior(FocusBehavior::NEVER);

  // TODO(brandosocarras, b/360150790): Create and use 'On'/'Off' strings.
  const bool is_qs_enabled = nearby_share_delegate_->IsEnabled();
  toggle_row_->AddLabelRow(is_qs_enabled ? u"On" : u"Off", /*start_inset=*/0);
  toggle_row_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *toggle_row_->text_label());

  auto toggle_switch = std::make_unique<Switch>(base::BindRepeating(
      &NearbyShareDetailedViewImpl::OnQuickShareToggleClicked,
      weak_factory_.GetWeakPtr()));
  quick_share_toggle_ = toggle_switch.get();
  toggle_switch->SetIsOn(is_qs_enabled);
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
  CreateHiddenRow();
  CreateEveryoneRow();
  FormatVisibilitySelectionContainer(
      nearby_share_delegate_->IsHighVisibilityOn());
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
  your_devices_row_->SetFocusBehavior(FocusBehavior::NEVER);
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
  contacts_row_->SetFocusBehavior(FocusBehavior::NEVER);
}

void NearbyShareDetailedViewImpl::CreateHiddenRow() {
  DCHECK(!hidden_row_);
  DCHECK(visibility_selection_container_);

  hidden_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  // TODO(brandosocarras, b/360150790): replace label, sublabel with IDS
  // strings.
  CreateVisibilityRow(hidden_row_, kQuickSettingsQuickShareHiddenIcon,
                      /*label=*/u"Hidden",
                      /*sublabel=*/u"No one can share with you");
  hidden_row_->SetFocusBehavior(FocusBehavior::NEVER);
}

void NearbyShareDetailedViewImpl::CreateVisibilityRow(
    HoverHighlightView* visibility_row,
    const gfx::VectorIcon& vector_icon,
    const std::u16string& label,
    const std::u16string& sublabel) {
  DCHECK(visibility_row);
  DCHECK(visibility_selection_container_);

  visibility_row->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(
          vector_icon, /*color_id=*/cros_tokens::kCrosSysOnSurface),
      label);
  visibility_row->SetSubText(sublabel);
  visibility_row->AddRightIcon(ui::ImageModel::FromVectorIcon(kCheckCircleIcon),
                               20);
  visibility_row->SetRightViewVisible(false);
}

void NearbyShareDetailedViewImpl::CreateEveryoneRow() {
  CHECK(!everyone_row_);
  CHECK(visibility_selection_container_);
  CHECK(nearby_share_delegate_);

  everyone_row_ = visibility_selection_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  everyone_row_->SetFocusBehavior(FocusBehavior::NEVER);
}

void NearbyShareDetailedViewImpl::FormatEveryoneRow(
    const ui::ColorId color_id,
    const bool in_high_visibility,
    const bool is_row_enabled) {
  everyone_toggle_ = nullptr;
  everyone_row_->Reset();
  // TODO(brandosocarras, b/360150790): Use IDS strings for label and sublabel.
  everyone_row_->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(kQuickSettingsQuickShareEveryoneIcon,
                                     /*color_id=*/color_id),
      u"Visible to everyone");
  everyone_row_->text_label()->SetEnabledColorId(color_id);
  everyone_row_->SetSubText(u"You will be visible to everyone for 5 minutes.");
  everyone_row_->sub_text_label()->SetEnabledColorId(color_id);
  auto toggle_switch = std::make_unique<Switch>(
      base::BindRepeating(&NearbyShareDetailedViewImpl::OnEveryoneToggleClicked,
                          weak_factory_.GetWeakPtr()));
  everyone_toggle_ = toggle_switch.get();
  toggle_switch->SetIsOn(
      in_high_visibility ||
      nearby_share_delegate_->IsEnableHighVisibilityRequestActive());
  everyone_row_->AddRightView(toggle_switch.release());
  everyone_row_->SetEnabled(is_row_enabled);

  // ChromeVox users will just use the toggle switch to toggle.
  everyone_row_->text_label()->GetViewAccessibility().SetIsIgnored(true);
}

void NearbyShareDetailedViewImpl::FormatVisibilitySelectionContainer(
    const bool in_high_visibility) {
  const bool is_quick_share_enabled = nearby_share_delegate_->IsEnabled();
  const bool is_background_visibility_enabled =
      is_quick_share_enabled && !in_high_visibility;

  const ui::ColorId background_visibility_row_color =
      is_background_visibility_enabled ? cros_tokens::kCrosSysOnSurface
                                       : cros_tokens::kCrosSysDisabled;
  const ui::ColorId everyone_row_color = is_quick_share_enabled
                                             ? cros_tokens::kCrosSysOnSurface
                                             : cros_tokens::kCrosSysDisabled;

  FormatVisibilityRow(your_devices_row_,
                      kQuickSettingsQuickShareYourDevicesIcon,
                      /*label=*/u"Your devices",
                      /*sublabel=*/u"Only devices signed into test@gmail.com",
                      /*color_id=*/background_visibility_row_color,
                      /*is_row_enabled=*/is_background_visibility_enabled);
  FormatVisibilityRow(contacts_row_, kQuickSettingsQuickShareContactsIcon,
                      /*label=*/u"Contacts",
                      /*sublabel=*/u"Only your contacts with a Google Account",
                      /*color_id=*/background_visibility_row_color,
                      /*is_row_enabled=*/is_background_visibility_enabled);
  FormatVisibilityRow(hidden_row_, kQuickSettingsQuickShareHiddenIcon,
                      /*label=*/u"Hidden",
                      /*sublabel=*/u"No one can share with you",
                      /*color_id=*/background_visibility_row_color,
                      /*is_row_enabled=*/is_background_visibility_enabled);
  FormatEveryoneRow(/*color_id=*/everyone_row_color, in_high_visibility,
                    /*is_row_enabled=*/is_quick_share_enabled);
  SetCheckCircle(in_high_visibility);
}

void NearbyShareDetailedViewImpl::OnSettingsButtonClicked() {
  CloseBubble();
  Shell::Get()->system_tray_model()->client()->ShowNearbyShareSettings();
}

void NearbyShareDetailedViewImpl::OnQuickShareToggleClicked() {
  CHECK(nearby_share_delegate_);
  CHECK(quick_share_toggle_);

  if (nearby_share_delegate_->IsEnableHighVisibilityRequestActive()) {
    quick_share_toggle_->SetIsOn(true);
    return;
  }

  const bool new_enabled_state = !nearby_share_delegate_->IsEnabled();
  // TODO(brandosocarras, b/360150790): Create and use 'On'/'Off' strings.
  toggle_row_->text_label()->SetText(new_enabled_state ? u"On" : u"Off");
  nearby_share_delegate_->SetEnabled(new_enabled_state);
  quick_share_toggle_->SetIsOn(new_enabled_state);
  FormatVisibilitySelectionContainer(/*in_high_visibility=*/false);
}

void NearbyShareDetailedViewImpl::OnYourDevicesSelected() {
  CHECK(nearby_share_delegate_);
  nearby_share_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kYourDevices);
  FormatVisibilitySelectionContainer(
      nearby_share_delegate_->IsHighVisibilityOn());
}

void NearbyShareDetailedViewImpl::OnContactsSelected() {
  CHECK(nearby_share_delegate_);
  nearby_share_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kAllContacts);
  FormatVisibilitySelectionContainer(
      nearby_share_delegate_->IsHighVisibilityOn());
}

void NearbyShareDetailedViewImpl::OnHiddenSelected() {
  CHECK(nearby_share_delegate_);
  nearby_share_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kNoOne);
  FormatVisibilitySelectionContainer(
      nearby_share_delegate_->IsHighVisibilityOn());
}

void NearbyShareDetailedViewImpl::OnEveryoneToggleClicked() {
  CHECK(nearby_share_delegate_);
  CHECK(everyone_toggle_);

  if (nearby_share_delegate_->IsEnableHighVisibilityRequestActive()) {
    everyone_toggle_->SetIsOn(true);
    return;
  }
  const bool in_high_visibility = nearby_share_delegate_->IsHighVisibilityOn();
  in_high_visibility ? nearby_share_delegate_->DisableHighVisibility()
                     : nearby_share_delegate_->EnableHighVisibility();
  FormatVisibilitySelectionContainer(!in_high_visibility);
}

void NearbyShareDetailedViewImpl::SetCheckCircle(
    const bool in_high_visibility) {
  CHECK(nearby_share_delegate_);

  if (!nearby_share_delegate_->IsEnabled() || in_high_visibility) {
    your_devices_row_->SetRightViewVisible(false);
    contacts_row_->SetRightViewVisible(false);
    hidden_row_->SetRightViewVisible(false);
    return;
  }

  switch (nearby_share_delegate_->GetVisibility()) {
    case ::nearby_share::mojom::Visibility::kYourDevices:
      CHECK(your_devices_row_);
      your_devices_row_->SetRightViewVisible(true);
      break;
    case ::nearby_share::mojom::Visibility::kAllContacts:
      CHECK(contacts_row_);
      contacts_row_->SetRightViewVisible(true);
      break;
    case ::nearby_share::mojom::Visibility::kNoOne:
      CHECK(hidden_row_);
      hidden_row_->SetRightViewVisible(true);
      break;
    default:
      break;
  }
}

BEGIN_METADATA(NearbyShareDetailedViewImpl)
END_METADATA

}  // namespace ash
