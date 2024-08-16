// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_detailed_view.h"

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/hotspot/hotspot_icon.h"
#include "ash/system/hotspot/hotspot_icon_animation.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotInfoPtr;
using hotspot_config::mojom::HotspotState;

namespace {

// Used for setting the insets of broader hotspot entry row.
constexpr auto kToggleRowTriViewInsets = gfx::Insets::VH(8, 24);

bool IsEnabledOrEnabling(HotspotState state) {
  return state == HotspotState::kEnabled || state == HotspotState::kEnabling;
}

bool CanToggleHotspot(HotspotState state, HotspotAllowStatus allow_status) {
  if (state == HotspotState::kDisabling) {
    return false;
  }
  if (state == HotspotState::kEnabling || state == HotspotState::kEnabled) {
    return true;
  }
  return allow_status == HotspotAllowStatus::kAllowed;
}

}  // namespace

HotspotDetailedView::HotspotDetailedView(
    DetailedViewDelegate* detailed_view_delegate,
    Delegate* delegate)
    : TrayDetailedView(detailed_view_delegate), delegate_(delegate) {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_HOTSPOT);
  CreateScrollableList();
  CreateContainer();
}

HotspotDetailedView::~HotspotDetailedView() {
  Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
}

void HotspotDetailedView::UpdateViewForHotspot(HotspotInfoPtr hotspot_info) {
  if (hotspot_info->state == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->AddObserver(this);
  } else if (state_ == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
  }

  if (state_ != hotspot_info->state) {
    state_ = hotspot_info->state;
    UpdateIcon();
  }

  UpdateSubText(hotspot_info);
  allow_status_ = hotspot_info->allow_status;
  UpdateToggleState(hotspot_info->state, hotspot_info->allow_status);
  UpdateExtraIcon(hotspot_info->allow_status);
}

void HotspotDetailedView::HandleViewClicked(views::View* view) {
  // Handle clicks on the on/off toggle row.
  if (view == entry_row_ && CanToggleHotspot(state_, allow_status_)) {
    // The toggle button has the old state, so switch to the opposite state.
    ToggleHotspot(!toggle_->GetIsOn());
    return;
  }
}

void HotspotDetailedView::CreateExtraTitleRowButtons() {
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  CHECK(!settings_button_);
  settings_button_ = CreateSettingsButton(
      base::BindRepeating(&HotspotDetailedView::OnSettingsClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_HOTSPOT_DETAILED_VIEW_HOTSPOT_SETTINGS);
  settings_button_->SetState(TrayPopupUtils::CanOpenWebUISettings()
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
  settings_button_->SetID(
      static_cast<int>(HotspotDetailedViewChildId::kSettingsButton));
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void HotspotDetailedView::CreateContainer() {
  row_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kAllRounded));
  // Ensure the HoverHighlightView ink drop fills the whole container.
  row_container_->SetBorderInsets(gfx::Insets());

  entry_row_ = row_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  entry_row_->SetID(static_cast<int>(HotspotDetailedViewChildId::kEntryRow));

  // The icon image and label text depend on whether hotspot is enabled. They
  // are set in UpdateViewForHotspot().
  auto hotspot_icon = std::make_unique<views::ImageView>();
  hotspot_icon->SetID(
      static_cast<int>(HotspotDetailedViewChildId::kHotspotIcon));
  hotspot_icon->SetImage(ui::ImageModel::FromVectorIcon(
      kHotspotOffIcon, cros_tokens::kCrosSysOnSurface));
  hotspot_icon_ = hotspot_icon.get();
  entry_row_->AddViewAndLabel(std::move(hotspot_icon), u"");
  const std::u16string text_label = l10n_util::GetStringFUTF16(
      IDS_ASH_HOTSPOT_DETAILED_VIEW_TITLE, ui::GetChromeOSDeviceName());
  entry_row_->text_label()->SetText(text_label);
  entry_row_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *entry_row_->text_label());
  entry_row_->GetViewAccessibility().SetName(text_label);

  auto toggle = std::make_unique<Switch>(base::BindRepeating(
      &HotspotDetailedView::OnToggleClicked, weak_factory_.GetWeakPtr()));
  toggle->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_HOTSPOT_DETAILED_VIEW_TOGGLE_A11Y_TEXT));
  toggle->SetID(static_cast<int>(HotspotDetailedViewChildId::kToggle));
  toggle->SetProperty(views::kElementIdentifierKey,
                      kHotspotDetailedViewToggleElementId);
  toggle_ = toggle.get();
  entry_row_->AddRightView(toggle.release());

  auto extra_icon = std::make_unique<views::ImageView>();
  extra_icon->SetVisible(false);
  extra_icon->SetID(static_cast<int>(HotspotDetailedViewChildId::kExtraIcon));
  extra_icon_ = extra_icon.get();
  entry_row_->AddAdditionalRightView(extra_icon.release());

  // Allow the row to be taller than a typical tray menu item.
  entry_row_->SetExpandable(true);
  entry_row_->tri_view()->SetInsets(kToggleRowTriViewInsets);
}

void HotspotDetailedView::OnSettingsClicked() {
  CloseBubble();  // Delete `this`.
  Shell::Get()->system_tray_model()->client()->ShowHotspotSubpage();
}

void HotspotDetailedView::OnToggleClicked() {
  // The toggle button already has the new state after a click.
  ToggleHotspot(toggle_->GetIsOn());
}

void HotspotDetailedView::ToggleHotspot(bool new_state) {
  delegate_->OnToggleClicked(new_state);
}

void HotspotDetailedView::HotspotIconChanged() {
  UpdateIcon();
}

void HotspotDetailedView::UpdateIcon() {
  hotspot_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      hotspot_icon::GetIconForHotspot(state_), cros_tokens::kCrosSysOnSurface));
}

void HotspotDetailedView::UpdateToggleState(
    const HotspotState& state,
    const HotspotAllowStatus& allow_status) {
  toggle_->SetEnabled(CanToggleHotspot(state, allow_status));
  const bool is_enabled_or_enabling = IsEnabledOrEnabling(state);
  toggle_->SetIsOn(is_enabled_or_enabling);
  entry_row_->SetAccessibilityState(
      is_enabled_or_enabling
          ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
          : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
}

void HotspotDetailedView::UpdateSubText(const HotspotInfoPtr& hotspot_info) {
  std::u16string sub_text;
  switch (hotspot_info->state) {
    case HotspotState::kEnabled: {
      uint32_t client_count = hotspot_info->client_count;
      if (client_count == 0) {
        sub_text = l10n_util::GetStringUTF16(
            IDS_ASH_HOTSPOT_DETAILED_VIEW_ON_NO_CONNECTED_DEVICES);
      } else if (client_count == 1) {
        sub_text = l10n_util::GetStringUTF16(
            IDS_ASH_HOTSPOT_ON_MESSAGE_ONE_CONNECTED_DEVICE);
      } else {
        sub_text = l10n_util::GetStringFUTF16(
            IDS_ASH_HOTSPOT_ON_MESSAGE_MULTIPLE_CONNECTED_DEVICES,
            base::NumberToString16(client_count));
      }
      break;
    }
    case HotspotState::kDisabled: {
      const HotspotAllowStatus allow_status = hotspot_info->allow_status;
      if (allow_status == HotspotAllowStatus::kDisallowedNoMobileData) {
        sub_text = l10n_util::GetStringUTF16(
            IDS_ASH_HOTSPOT_DETAILED_VIEW_SUBLABEL_NO_MOBILE_DATA);
      }
      break;
    }
    case HotspotState::kEnabling:
      sub_text = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_ENABLING);
      break;
    case HotspotState::kDisabling:
      sub_text = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_DISABLING);
      break;
  }

  if (!sub_text.empty()) {
    entry_row_->SetSubText(sub_text);
    entry_row_->sub_text_label()->SetVisible(true);
    entry_row_->GetViewAccessibility().SetDescription(sub_text);
    if (hotspot_info->state != HotspotState::kEnabled) {
      // If hotspot is not enabled, no need to set primary color for the status
      // sublabel text.
      return;
    }
    // Set color for the subtext that shows hotspot is connected.
    entry_row_->sub_text_label()->SetEnabledColorId(
        cros_tokens::kCrosSysPositive);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                          *entry_row_->sub_text_label());
    return;
  }
  // If no subtext is set, previous subtext should be hidden.
  if (entry_row_->sub_text_label()) {
    entry_row_->sub_text_label()->SetVisible(false);
    entry_row_->GetViewAccessibility().SetDescription(
        std::u16string(),
        ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  }
}

void HotspotDetailedView::UpdateExtraIcon(
    const HotspotAllowStatus& allow_status) {
  if (allow_status == HotspotAllowStatus::kAllowed ||
      allow_status == HotspotAllowStatus::kDisallowedNoMobileData) {
    extra_icon_->SetVisible(false);
    return;
  }

  extra_icon_->SetVisible(true);
  bool use_managed_icon =
      allow_status == HotspotAllowStatus::kDisallowedByPolicy;
  extra_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      use_managed_icon ? kSystemTrayManagedIcon : kUnifiedMenuInfoIcon,
      kColorAshIconColorPrimary));
  const std::u16string tooltip = l10n_util::GetStringUTF16(
      use_managed_icon
          ? IDS_ASH_HOTSPOT_DETAILED_VIEW_INFO_TOOLTIP_PROHIBITED_BY_POLICY
          : IDS_ASH_HOTSPOT_DETAILED_VIEW_INFO_TOOLTIP_MOBILE_DATA_NOT_SUPPORTED);
  extra_icon_->SetFocusBehavior(FocusBehavior::ALWAYS);
  extra_icon_->SetTooltipText(tooltip);
  extra_icon_->GetViewAccessibility().SetName(tooltip);
}

BEGIN_METADATA(HotspotDetailedView)
END_METADATA

}  // namespace ash
