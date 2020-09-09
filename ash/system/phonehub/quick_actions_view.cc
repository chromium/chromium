// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_actions_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kQuickActionsItemSpacing = 36;
constexpr int kQuickActionsItemLabelSize = 14;
constexpr gfx::Insets kQuickActionsViewPadding(16, 4);

void ConfigureLabel(views::Label* label, bool is_primary) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->set_can_process_events_within_subtree(false);

  auto type = is_primary
                  ? AshColorProvider::ContentLayerType::kTextColorPrimary
                  : AshColorProvider::ContentLayerType::kTextColorSecondary;
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(type));

  gfx::Font default_font;
  gfx::Font label_font = default_font.Derive(
      kQuickActionsItemLabelSize - default_font.GetFontSize(),
      gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
}

}  // namespace

QuickActionsItem::QuickActionsItem(views::ButtonListener* listener,
                                   const gfx::VectorIcon& vector_icon,
                                   int label_id)
    : icon_button_(new FeaturePodIconButton(listener, true /* is_togglable */)),
      label_(new views::Label),
      sub_label_(new views::Label) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_button_ = AddChildView(std::make_unique<FeaturePodIconButton>(
      listener, true /* is_togglable */));
  icon_button_->SetVectorIcon(vector_icon);

  auto* label_view = AddChildView(std::make_unique<views::View>());
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));

  label_ = label_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(label_id)));
  ConfigureLabel(label_, true /* is_primary */);

  sub_label_ = label_view->AddChildView(std::make_unique<views::Label>());
  ConfigureLabel(sub_label_, false /* is_primary */);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

QuickActionsItem::~QuickActionsItem() = default;

void QuickActionsItem::SetSubLabel(const base::string16& sub_label) {
  sub_label_->SetText(sub_label);
}

void QuickActionsItem::SetIconTooltip(const base::string16& text) {
  icon_button_->SetTooltipText(text);
}

void QuickActionsItem::SetToggled(bool toggled) {
  icon_button_->SetToggled(toggled);
}

const base::string16& QuickActionsItem::GetItemLabel() const {
  return label_->GetText();
}

bool QuickActionsItem::HasFocus() const {
  return icon_button_->HasFocus() || label_->HasFocus() ||
         sub_label_->HasFocus();
}

void QuickActionsItem::RequestFocus() {
  icon_button_->RequestFocus();
}

const char* QuickActionsItem::GetClassName() const {
  return "QuickActionsItem";
}

QuickActionsView::QuickActionsView() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQuickActionsViewPadding,
      kQuickActionsItemSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  enable_hotspot_ = AddChildView(std::make_unique<QuickActionsItem>(
      this, kSystemMenuPhoneIcon, IDS_ASH_PHONE_HUB_ENABLE_HOTSPOT_TITLE));
  silence_phone_ = AddChildView(std::make_unique<QuickActionsItem>(
      this, kSystemMenuPhoneIcon, IDS_ASH_PHONE_HUB_SILENCE_PHONE_TITLE));
  locate_phone_ = AddChildView(std::make_unique<QuickActionsItem>(
      this, kSystemMenuPhoneIcon, IDS_ASH_PHONE_HUB_LOCATE_PHONE_TITLE));

  Update();
}

QuickActionsView::~QuickActionsView() = default;

void QuickActionsView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // TODO(leandre): implement quick actions button functionality.
  if (sender == enable_hotspot_->icon_button()) {
    UpdateItem(enable_hotspot_, !enable_hotspot_->IsToggled());
  } else if (sender == silence_phone_->icon_button()) {
    UpdateItem(silence_phone_, !silence_phone_->IsToggled());
  } else if (sender == locate_phone_->icon_button()) {
    UpdateItem(locate_phone_, !locate_phone_->IsToggled());
  } else {
    NOTREACHED();
  }
}

void QuickActionsView::Update() {
  // TODO(leandre): Update items according to phone status.
  UpdateItem(enable_hotspot_, false);
  UpdateItem(silence_phone_, true);
  UpdateItem(locate_phone_, false);
}

void QuickActionsView::UpdateItem(QuickActionsItem* item, bool is_enabled) {
  item->SetToggled(is_enabled);
  item->SetSubLabel(l10n_util::GetStringUTF16(
      is_enabled ? IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ON_STATE
                 : IDS_ASH_PHONE_HUB_QUICK_ACTIONS_OFF_STATE));
  int state_text_id =
      is_enabled ? IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ENABLED_STATE_TOOLTIP
                 : IDS_ASH_PHONE_HUB_QUICK_ACTIONS_DISABLED_STATE_TOOLTIP;
  base::string16 tooltip_state =
      l10n_util::GetStringFUTF16(state_text_id, item->GetItemLabel());
  item->SetIconTooltip(
      l10n_util::GetStringFUTF16(IDS_ASH_PHONE_HUB_QUICK_ACTIONS_TOGGLE_TOOLTIP,
                                 item->GetItemLabel(), tooltip_state));
}

}  // namespace ash
