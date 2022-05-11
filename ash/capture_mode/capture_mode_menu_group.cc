// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_menu_group.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/ranges/algorithm.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr auto kMenuGroupPadding = gfx::Insets::VH(8, 0);

constexpr auto kMenuHeaderPadding = gfx::Insets::VH(8, 16);

constexpr auto kOptionPadding = gfx::Insets::TLBR(8, 52, 8, 16);

constexpr auto kMenuItemPadding = gfx::Insets::TLBR(10, 52, 10, 16);

constexpr int kSpaceBetweenMenuItem = 0;

constexpr gfx::Size kIconSize{20, 20};

void ConfigLabelView(views::Label* label_view) {
  label_view->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  label_view->SetBackgroundColor(SK_ColorTRANSPARENT);
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
}

views::BoxLayout* CreateAndInitBoxLayoutForView(views::View* view) {
  auto* box_layout = view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  return box_layout;
}

void SetInkDropForButton(views::Button* button) {
  StyleUtil::SetUpInkDropForButton(button, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRectHighlightPathGenerator(button);
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeMenuHeader:

// The header of the menu group, which has an icon and a text label. Not user
// interactable.
class CaptureModeMenuHeader
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeMenuHeader);

  CaptureModeMenuHeader(const gfx::VectorIcon& icon,
                        std::u16string header_laber,
                        bool managed_by_policy)
      : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(header_laber)))),
        managed_icon_view_(
            managed_by_policy
                ? AddChildView(std::make_unique<views::ImageView>())
                : nullptr) {
    icon_view_->SetImageSize(kIconSize);
    icon_view_->SetPreferredSize(kIconSize);
    const auto icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    icon_view_->SetImage(gfx::CreateVectorIcon(icon, icon_color));

    if (managed_icon_view_) {
      managed_icon_view_->SetImageSize(kIconSize);
      managed_icon_view_->SetPreferredSize(kIconSize);
      managed_icon_view_->SetImage(
          gfx::CreateVectorIcon(kCaptureModeManagedIcon, icon_color));
      managed_icon_view_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MANAGED_BY_POLICY));
    }

    SetBorder(views::CreateEmptyBorder(kMenuHeaderPadding));
    ConfigLabelView(label_view_);
    auto* box_layout = CreateAndInitBoxLayoutForView(this);
    box_layout->SetFlexForView(label_view_, 1);
  }

  CaptureModeMenuHeader(const CaptureModeMenuHeader&) = delete;
  CaptureModeMenuHeader& operator=(const CaptureModeMenuHeader&) = delete;
  ~CaptureModeMenuHeader() override = default;

  const std::u16string& GetHeaderLabel() const {
    return label_view_->GetText();
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    View::GetAccessibleNodeData(node_data);
    node_data->SetName(GetHeaderLabel());
    node_data->role = ax::mojom::Role::kHeader;
  }

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  views::ImageView* icon_view_;
  views::Label* label_view_;
  // `nullptr` if the menu group is not for a setting that is managed by a
  // policy.
  views::ImageView* managed_icon_view_;
};

BEGIN_METADATA(CaptureModeMenuHeader, views::View)
END_METADATA

// -----------------------------------------------------------------------------
// CaptureModeMenuItem:

// A button which has a text label. Its behavior on click can be customized.
// For selecting folder, a folder window will be opened on click.
class CaptureModeMenuItem
    : public views::Button,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeMenuItem);

  CaptureModeMenuItem(views::Button::PressedCallback callback,
                      std::u16string item_label)
      : views::Button(callback),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(item_label)))) {
    SetBorder(views::CreateEmptyBorder(kMenuItemPadding));
    ConfigLabelView(label_view_);
    CreateAndInitBoxLayoutForView(this);
    SetInkDropForButton(this);
    GetViewAccessibility().OverrideIsLeaf(true);
    SetAccessibleName(label_view_->GetText());
  }

  CaptureModeMenuItem(const CaptureModeMenuItem&) = delete;
  CaptureModeMenuItem& operator=(const CaptureModeMenuItem&) = delete;
  ~CaptureModeMenuItem() override = default;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  views::Label* label_view_;
};

BEGIN_METADATA(CaptureModeMenuItem, views::Button)
END_METADATA

// -----------------------------------------------------------------------------
// CaptureModeOption:

// A button which represents an option of the menu group. It has a text label
// and a checked icon. The checked icon will be shown on button click and any
// other option's checked icon will be set to invisible in the meanwhile. One
// and only one checked icon is visible in the menu group.
class CaptureModeOption
    : public views::Button,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeOption);

  CaptureModeOption(views::Button::PressedCallback callback,
                    std::u16string option_label,
                    int option_id,
                    bool checked,
                    bool enabled)
      : views::Button(callback),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(option_label)))),
        checked_icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        id_(option_id) {
    checked_icon_view_->SetImageSize(kIconSize);
    checked_icon_view_->SetPreferredSize(kIconSize);

    SetBorder(views::CreateEmptyBorder(kOptionPadding));
    ConfigLabelView(label_view_);
    auto* box_layout = CreateAndInitBoxLayoutForView(this);
    box_layout->SetFlexForView(label_view_, 1);
    SetInkDropForButton(this);
    GetViewAccessibility().OverrideIsLeaf(true);
    SetAccessibleName(GetOptionLabel());

    checked_icon_view_->SetVisible(checked);

    // Calling `SetEnabled()` will result in calling `UpdateState()` only when
    // the state changes, but by default the view's state is enabled, so we only
    // need to call `UpdateState()` explicitly if `enabled` is true.
    if (enabled)
      UpdateState();
    else
      SetEnabled(false);
  }

  CaptureModeOption(const CaptureModeOption&) = delete;
  CaptureModeOption& operator=(const CaptureModeOption&) = delete;
  ~CaptureModeOption() override = default;

  int id() const { return id_; }

  const std::u16string& GetOptionLabel() const {
    return label_view_->GetText();
  }

  void SetOptionLabel(std::u16string option_label) {
    SetAccessibleName(option_label);
    label_view_->SetText(std::move(option_label));
  }

  void SetOptionChecked(bool checked) {
    checked_icon_view_->SetVisible(checked);
  }

  bool IsOptionChecked() { return checked_icon_view_->GetVisible(); }

  // views::Button:
  void StateChanged(ButtonState old_state) override { UpdateState(); }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    Button::GetAccessibleNodeData(node_data);
    node_data->SetName(GetOptionLabel());
    node_data->role = ax::mojom::Role::kRadioButton;
    node_data->SetCheckedState(IsOptionChecked()
                                   ? ax::mojom::CheckedState::kTrue
                                   : ax::mojom::CheckedState::kFalse);
  }

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  // Dims out the label and the checked icon if this view is disabled.
  void UpdateState() {
    auto* provider = AshColorProvider::Get();
    const auto label_enabled_color = provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    const auto icon_enabled_color = provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonLabelColorBlue);
    const bool is_disabled = GetState() == STATE_DISABLED;
    label_view_->SetEnabledColor(
        is_disabled ? provider->GetDisabledColor(label_enabled_color)
                    : label_enabled_color);
    checked_icon_view_->SetImage(gfx::CreateVectorIcon(
        kHollowCheckCircleIcon,
        is_disabled ? provider->GetDisabledColor(icon_enabled_color)
                    : icon_enabled_color));
  }

  views::Label* label_view_;
  views::ImageView* checked_icon_view_;
  const int id_;
};

BEGIN_METADATA(CaptureModeOption, views::Button)
END_METADATA

// -----------------------------------------------------------------------------
// CaptureModeMenuGroup:

CaptureModeMenuGroup::CaptureModeMenuGroup(Delegate* delegate,
                                           const gfx::VectorIcon& header_icon,
                                           std::u16string header_label,
                                           bool managed_by_policy)
    : delegate_(delegate),
      menu_header_(AddChildView(
          std::make_unique<CaptureModeMenuHeader>(header_icon,
                                                  std::move(header_label),
                                                  managed_by_policy))) {
  options_container_ = AddChildView(std::make_unique<views::View>());
  options_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMenuGroupPadding,
      kSpaceBetweenMenuItem));
}

CaptureModeMenuGroup::~CaptureModeMenuGroup() = default;

void CaptureModeMenuGroup::AddOption(std::u16string option_label,
                                     int option_id) {
  options_.push_back(
      options_container_->AddChildView(std::make_unique<CaptureModeOption>(
          base::BindRepeating(&CaptureModeMenuGroup::HandleOptionClick,
                              base::Unretained(this), option_id),
          std::move(option_label), option_id,
          /*checked=*/delegate_->IsOptionChecked(option_id),
          /*enabled=*/delegate_->IsOptionEnabled(option_id))));
}

void CaptureModeMenuGroup::DeleteOptions() {
  for (CaptureModeOption* option : options_)
    options_container_->RemoveChildViewT(option);
  options_.clear();
}

void CaptureModeMenuGroup::AddOrUpdateExistingOption(
    std::u16string option_label,
    int option_id) {
  auto* option = GetOptionById(option_id);

  if (option) {
    option->SetOptionLabel(std::move(option_label));
    return;
  }

  AddOption(std::move(option_label), option_id);
}

void CaptureModeMenuGroup::RefreshOptionsSelections() {
  for (auto* option : options_) {
    option->SetOptionChecked(delegate_->IsOptionChecked(option->id()));
    option->SetEnabled(delegate_->IsOptionEnabled(option->id()));
  }
}

void CaptureModeMenuGroup::RemoveOptionIfAny(int option_id) {
  auto* option = GetOptionById(option_id);
  if (!option)
    return;

  options_container_->RemoveChildViewT(option);
  base::Erase(options_, option);
}

void CaptureModeMenuGroup::AddMenuItem(views::Button::PressedCallback callback,
                                       std::u16string item_label) {
  menu_items_.push_back(views::View::AddChildView(
      std::make_unique<CaptureModeMenuItem>(callback, std::move(item_label))));
}

bool CaptureModeMenuGroup::IsOptionChecked(int option_id) const {
  auto* option = GetOptionById(option_id);
  return option && option->IsOptionChecked();
}

bool CaptureModeMenuGroup::IsOptionEnabled(int option_id) const {
  auto* option = GetOptionById(option_id);
  return option && option->GetEnabled();
}

void CaptureModeMenuGroup::AppendHighlightableItems(
    std::vector<CaptureModeSessionFocusCycler::HighlightableView*>&
        highlightable_items) {
  // The camera menu group can be hidden if there are no cameras connected. In
  // this case no items in this group should be highlightable.
  if (!GetVisible())
    return;

  highlightable_items.push_back(menu_header_);
  for (auto* option : options_) {
    if (option->GetEnabled())
      highlightable_items.push_back(option);
  }
  for (auto* menu_item : menu_items_)
    highlightable_items.push_back(menu_item);
}

views::View* CaptureModeMenuGroup::GetOptionForTesting(int option_id) {
  return GetOptionById(option_id);
}

views::View* CaptureModeMenuGroup::GetSelectFolderMenuItemForTesting() {
  DCHECK_EQ(1u, menu_items_.size());
  return menu_items_[0];
}

std::u16string CaptureModeMenuGroup::GetOptionLabelForTesting(
    int option_id) const {
  auto* option = GetOptionById(option_id);
  DCHECK(option);
  return option->GetOptionLabel();
}

CaptureModeOption* CaptureModeMenuGroup::GetOptionById(int option_id) const {
  auto iter =
      base::ranges::find_if(options_, [option_id](CaptureModeOption* option) {
        return option->id() == option_id;
      });
  return iter == options_.end() ? nullptr : *iter;
}

void CaptureModeMenuGroup::HandleOptionClick(int option_id) {
  DCHECK(GetOptionById(option_id));

  // The order here matters. We need to tell the delegate first about a change
  // in the selection, before we refresh the checked icons, since for that we
  // need to query the delegate.
  delegate_->OnOptionSelected(option_id);
  RefreshOptionsSelections();
}

BEGIN_METADATA(CaptureModeMenuGroup, views::View)
END_METADATA

}  // namespace ash
