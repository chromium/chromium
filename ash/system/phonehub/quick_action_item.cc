// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_action_item.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kQuickActionItemLabelSize = 14;

void ConfigureLabel(views::Label* label, bool is_primary) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetCanProcessEventsWithinSubtree(false);

  auto type = is_primary
                  ? AshColorProvider::ContentLayerType::kTextColorPrimary
                  : AshColorProvider::ContentLayerType::kTextColorSecondary;
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(type));

  gfx::Font default_font;
  gfx::Font label_font = default_font.Derive(
      kQuickActionItemLabelSize - default_font.GetFontSize(), gfx::Font::NORMAL,
      gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
}

}  // namespace

QuickActionItem::QuickActionItem(Delegate* delegate,
                                 int label_id,
                                 const gfx::VectorIcon& icon_on,
                                 const gfx::VectorIcon& icon_off)
    : delegate_(delegate), icon_on_(icon_on), icon_off_(icon_off) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_button_ = AddChildView(
      std::make_unique<FeaturePodIconButton>(this, true /* is_togglable */));

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

QuickActionItem::QuickActionItem(Delegate* delegate,
                                 int label_id,
                                 const gfx::VectorIcon& icon)
    : QuickActionItem(delegate, label_id, icon, icon) {}

QuickActionItem::~QuickActionItem() = default;

void QuickActionItem::SetSubLabel(const base::string16& sub_label) {
  sub_label_->SetText(sub_label);
}

void QuickActionItem::SetIconTooltip(const base::string16& text) {
  icon_button_->SetTooltipText(text);
}

void QuickActionItem::SetToggled(bool toggled) {
  icon_button_->SetToggled(toggled);
  icon_button_->SetVectorIcon(toggled ? icon_on_ : icon_off_);
}

bool QuickActionItem::IsToggled() const {
  return icon_button_->toggled();
}

const base::string16& QuickActionItem::GetItemLabel() const {
  return label_->GetText();
}

void QuickActionItem::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  icon_button_->SetEnabled(enabled);

  if (!enabled) {
    label_->SetEnabledColor(
        AshColorProvider::GetDisabledColor(label_->GetEnabledColor()));
    sub_label_->SetEnabledColor(
        AshColorProvider::GetDisabledColor(sub_label_->GetEnabledColor()));

    sub_label_->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE));
    icon_button_->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE_TOOLTIP,
        GetItemLabel()));
  } else {
    ConfigureLabel(label_, true /* is_primary */);
    ConfigureLabel(sub_label_, false /* is_primary */);
  }
}

void QuickActionItem::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  delegate_->OnButtonPressed(IsToggled());
}

bool QuickActionItem::HasFocus() const {
  return icon_button_->HasFocus() || label_->HasFocus() ||
         sub_label_->HasFocus();
}

void QuickActionItem::RequestFocus() {
  icon_button_->RequestFocus();
}

const char* QuickActionItem::GetClassName() const {
  return "QuickActionItem";
}

}  // namespace ash
