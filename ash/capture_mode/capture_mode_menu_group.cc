// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_menu_group.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr auto kMenuGroupPadding = gfx::Insets::VH(8, 0);

constexpr auto kOptionPadding = gfx::Insets::VH(8, 16);
constexpr auto kIndentedOptionPadding = gfx::Insets::TLBR(8, 52, 8, 16);

constexpr auto kMenuItemPadding = gfx::Insets::VH(10, 16);
constexpr auto kIndentedMenuItemPadding = gfx::Insets::TLBR(10, 52, 10, 16);

constexpr int kSpaceBetweenMenuItem = 0;

void SetInkDropForButton(views::Button* button) {
  StyleUtil::SetUpInkDropForButton(button, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRectHighlightPathGenerator(button);
}

// Configures the size and visibility of the given `icon_view`.
void ConfigureIconView(views::ImageView* icon_view, bool is_visible) {
  DCHECK(icon_view);
  icon_view->SetImageSize(capture_mode::kSettingsIconSize);
  icon_view->SetPreferredSize(capture_mode::kSettingsIconSize);
  icon_view->SetVisible(is_visible);
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeMenuHeader:

// The header of the menu group, which has an icon and a text label. Not user
// interactable.
class CaptureModeMenuHeader
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView {
  METADATA_HEADER(CaptureModeMenuHeader, views::View)

 public:
  CaptureModeMenuHeader(const gfx::VectorIcon& icon,
                        std::u16string header_label,
                        bool managed_by_policy)
      : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(header_label)))),
        managed_icon_view_(
            managed_by_policy
                ? AddChildView(std::make_unique<views::ImageView>())
                : nullptr) {
    icon_view_->SetImageSize(capture_mode::kSettingsIconSize);
    icon_view_->SetPreferredSize(capture_mode::kSettingsIconSize);
    icon_view_->SetImage(
        ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconColor));

    if (managed_icon_view_) {
      managed_icon_view_->SetImageSize(capture_mode::kSettingsIconSize);
      managed_icon_view_->SetPreferredSize(capture_mode::kSettingsIconSize);
      managed_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
          kCaptureModeManagedIcon, kColorAshIconColorSecondary));
      managed_icon_view_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MANAGED_BY_POLICY));
    }

    SetBorder(views::CreateEmptyBorder(capture_mode::kSettingsMenuBorderSize));
    capture_mode_util::ConfigLabelView(label_view_);
    auto* box_layout = capture_mode_util::CreateAndInitBoxLayoutForView(this);
    box_layout->SetFlexForView(label_view_, 1);

    GetViewAccessibility().SetRole(ax::mojom::Role::kHeader);
    GetViewAccessibility().SetName(GetHeaderLabel());
  }

  CaptureModeMenuHeader(const CaptureModeMenuHeader&) = delete;
  CaptureModeMenuHeader& operator=(const CaptureModeMenuHeader&) = delete;
  ~CaptureModeMenuHeader() override = default;

  bool is_managed_by_policy() const { return !!managed_icon_view_; }

  const std::u16string& GetHeaderLabel() const {
    return label_view_->GetText();
  }

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  raw_ptr<views::ImageView> icon_view_;
  raw_ptr<views::Label> label_view_;
  // `nullptr` if the menu group is not for a setting that is managed by a
  // policy.
  raw_ptr<views::ImageView> managed_icon_view_;
};

BEGIN_METADATA(CaptureModeMenuHeader)
END_METADATA

// -----------------------------------------------------------------------------
// CaptureModeMenuItem:

// A button which has a text label. Its behavior on click can be customized.
// For selecting folder, a folder window will be opened on click.
class CaptureModeMenuItem
    : public views::Button,
      public CaptureModeSessionFocusCycler::HighlightableView {
  METADATA_HEADER(CaptureModeMenuItem, views::Button)

 public:
  // If `indented` is true, the content of this menu item will have some extra
  // padding from the left so that it appears indented. This is useful when this
  // item is added to a group that has a header, and it's desired to make it
  // appear to be pushed inside under the header.
  CaptureModeMenuItem(views::Button::PressedCallback callback,
                      std::u16string item_label,
                      bool indented,
                      bool enabled)
      : views::Button(std::move(callback)),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(item_label)))) {
    SetBorder(views::CreateEmptyBorder(indented ? kIndentedMenuItemPadding
                                                : kMenuItemPadding));
    capture_mode_util::ConfigLabelView(label_view_);
    capture_mode_util::CreateAndInitBoxLayoutForView(this);
    SetInkDropForButton(this);
    GetViewAccessibility().SetIsLeaf(true);
    GetViewAccessibility().SetName(label_view_->GetText());
    SetEnabled(enabled);
  }

  CaptureModeMenuItem(const CaptureModeMenuItem&) = delete;
  CaptureModeMenuItem& operator=(const CaptureModeMenuItem&) = delete;
  ~CaptureModeMenuItem() override = default;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  raw_ptr<views::Label> label_view_;
};

BEGIN_METADATA(CaptureModeMenuItem)
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
  METADATA_HEADER(CaptureModeOption, views::Button)

 public:
  // If `indented` is true, the content of this option will have some extra
  // padding from the left so that it appears indented. This is useful when this
  // option is added to a group that has a header, and it's desired to make it
  // appear to be pushed inside under the header.
  CaptureModeOption(views::Button::PressedCallback callback,
                    const gfx::VectorIcon* option_icon,
                    std::u16string option_label,
                    int option_id,
                    bool checked,
                    bool enabled,
                    bool indented)
      : views::Button(std::move(callback)),
        option_icon_(option_icon),
        option_icon_view_(
            option_icon_ ? AddChildView(std::make_unique<views::ImageView>())
                         : nullptr),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(option_label)))),
        checked_icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        id_(option_id) {
    if (option_icon_view_)
      ConfigureIconView(option_icon_view_, /*is_visible=*/true);
    ConfigureIconView(checked_icon_view_, /*is_visible=*/checked);
    SetBorder(views::CreateEmptyBorder(indented ? kIndentedOptionPadding
                                                : kOptionPadding));
    capture_mode_util::ConfigLabelView(label_view_);
    auto* box_layout = capture_mode_util::CreateAndInitBoxLayoutForView(this);
    box_layout->SetFlexForView(label_view_, 1);
    SetInkDropForButton(this);
    GetViewAccessibility().SetIsLeaf(true);
    GetViewAccessibility().SetName(GetOptionLabel());
    GetViewAccessibility().SetRole(ax::mojom::Role::kRadioButton);

    SetEnabled(enabled);
  }

  CaptureModeOption(const CaptureModeOption&) = delete;
  CaptureModeOption& operator=(const CaptureModeOption&) = delete;
  ~CaptureModeOption() override = default;

  int id() const { return id_; }

  const std::u16string& GetOptionLabel() const {
    return label_view_->GetText();
  }

  // If `icon` is `nullptr`, removes the `option_icon_view_` (if any).
  // Otherwise, a new image view will be created for the `option_icon_view_` (if
  // needed), and the given `icon` will be set.
  void SetOptionIcon(const gfx::VectorIcon* icon) {
    option_icon_ = icon;

    if (!option_icon_) {
      if (option_icon_view_) {
        RemoveChildViewT(option_icon_view_.get());
        option_icon_view_ = nullptr;
      }
      return;
    }

    if (!option_icon_view_) {
      option_icon_view_ =
          AddChildViewAt(std::make_unique<views::ImageView>(), 0);
      ConfigureIconView(option_icon_view_, /*is_visible=*/true);
    }

    MaybeUpdateOptionIconState();
  }

  void SetOptionLabel(std::u16string option_label) {
    GetViewAccessibility().SetName(option_label);
    label_view_->SetText(std::move(option_label));
  }

  void SetOptionChecked(bool checked) {
    checked_icon_view_->SetVisible(checked);
    GetViewAccessibility().SetCheckedState(
        checked ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
  }

  bool IsOptionChecked() { return checked_icon_view_->GetVisible(); }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    // Don't trigger `UpdateState` when the option is not added to the views
    // hierarchy yet, since we need to get the color from the widget's color
    // provider. When the option is added to the view hierarchy,
    // `OnThemeChanged` will be triggered and then `UpdateState` will be called.
    if (GetWidget())
      UpdateState();
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    UpdateState();
  }

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override { return this; }

 private:
  // Dims out the label and the checked icon if this view is disabled.
  void UpdateState() {
    MaybeUpdateOptionIconState();

    const bool is_disabled = GetState() == STATE_DISABLED;
    const auto* color_provider = GetColorProvider();
    const auto label_enabled_color =
        color_provider->GetColor(kColorAshTextColorPrimary);
    label_view_->SetEnabledColor(
        is_disabled ? ColorUtil::GetDisabledColor(label_enabled_color)
                    : label_enabled_color);

    const auto checked_icon_enabled_color =
        color_provider->GetColor(kColorAshButtonLabelColorBlue);
    checked_icon_view_->SetImage(gfx::CreateVectorIcon(
        kHollowCheckCircleIcon,
        is_disabled ? ColorUtil::GetDisabledColor(checked_icon_enabled_color)
                    : checked_icon_enabled_color));
  }

  void MaybeUpdateOptionIconState() {
    if (!option_icon_view_) {
      DCHECK(!option_icon_);
      return;
    }

    DCHECK(option_icon_);
    const bool is_disabled = GetState() == STATE_DISABLED;
    option_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *option_icon_, is_disabled ? kColorAshButtonIconDisabledColor
                                   : kColorAshButtonIconColor));
  }

  // An optional icon for the option. Non-null if present.
  raw_ptr<const gfx::VectorIcon> option_icon_ = nullptr;
  raw_ptr<views::ImageView> option_icon_view_ = nullptr;

  raw_ptr<views::Label> label_view_;
  raw_ptr<views::ImageView> checked_icon_view_;
  const int id_;
};

BEGIN_METADATA(CaptureModeOption)
END_METADATA

// -----------------------------------------------------------------------------
// CaptureModeMenuGroup:

CaptureModeMenuGroup::CaptureModeMenuGroup(
    Delegate* delegate,
    const gfx::Insets& inside_border_insets)
    : CaptureModeMenuGroup(delegate,
                           /*menu_header=*/nullptr,
                           inside_border_insets) {}

CaptureModeMenuGroup::CaptureModeMenuGroup(Delegate* delegate,
                                           const gfx::VectorIcon& header_icon,
                                           std::u16string header_label,
                                           bool managed_by_policy)
    : CaptureModeMenuGroup(
          delegate,
          std::make_unique<CaptureModeMenuHeader>(header_icon,
                                                  std::move(header_label),
                                                  managed_by_policy),
          kMenuGroupPadding) {}

CaptureModeMenuGroup::~CaptureModeMenuGroup() = default;

bool CaptureModeMenuGroup::IsManagedByPolicy() const {
  return menu_header_ && menu_header_->is_managed_by_policy();
}

void CaptureModeMenuGroup::AddOption(const gfx::VectorIcon* option_icon,
                                     std::u16string option_label,
                                     int option_id) {
  options_.push_back(
      options_container_->AddChildView(std::make_unique<CaptureModeOption>(
          base::BindRepeating(&CaptureModeMenuGroup::HandleOptionClick,
                              base::Unretained(this), option_id),
          option_icon, std::move(option_label), option_id,
          /*checked=*/delegate_->IsOptionChecked(option_id),
          /*enabled=*/delegate_->IsOptionEnabled(option_id),
          /*indented=*/!!menu_header_)));
}

void CaptureModeMenuGroup::DeleteOptions() {
  for (CaptureModeOption* option : options_)
    options_container_->RemoveChildViewT(option);
  options_.clear();
}

void CaptureModeMenuGroup::AddOrUpdateExistingOption(
    const gfx::VectorIcon* option_icon,
    std::u16string option_label,
    int option_id) {
  auto* option = GetOptionById(option_id);

  if (option) {
    option->SetOptionIcon(option_icon);
    option->SetOptionLabel(std::move(option_label));
    return;
  }

  AddOption(option_icon, std::move(option_label), option_id);
}

void CaptureModeMenuGroup::RefreshOptionsSelections() {
  for (ash::CaptureModeOption* option : options_) {
    option->SetOptionChecked(delegate_->IsOptionChecked(option->id()));
    option->SetEnabled(delegate_->IsOptionEnabled(option->id()));
  }
}

void CaptureModeMenuGroup::RemoveOptionIfAny(int option_id) {
  auto* option = GetOptionById(option_id);
  if (!option)
    return;

  options_container_->RemoveChildViewT(option);
  std::erase(options_, option);
}

void CaptureModeMenuGroup::AddMenuItem(views::Button::PressedCallback callback,
                                       std::u16string item_label,
                                       bool enabled) {
  menu_items_.push_back(
      views::View::AddChildView(std::make_unique<CaptureModeMenuItem>(
          std::move(callback), std::move(item_label),
          /*indented=*/!!menu_header_, enabled)));
}

bool CaptureModeMenuGroup::IsOptionChecked(int option_id) const {
  auto* option = GetOptionById(option_id);
  return option && option->IsOptionChecked();
}

views::View* CaptureModeMenuGroup::SetOptionCheckedForTesting(
    int option_id,
    bool checked) const {
  auto* option = GetOptionById(option_id);
  option->SetOptionChecked(checked);
  return option;
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

  if (menu_header_)
    highlightable_items.push_back(menu_header_);
  for (ash::CaptureModeOption* option : options_) {
    if (option->GetEnabled())
      highlightable_items.push_back(option);
  }
  for (ash::CaptureModeMenuItem* menu_item : menu_items_) {
    highlightable_items.push_back(menu_item);
  }
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

CaptureModeMenuGroup::CaptureModeMenuGroup(
    Delegate* delegate,
    std::unique_ptr<CaptureModeMenuHeader> menu_header,
    const gfx::Insets& inside_border_insets)
    : delegate_(delegate),
      menu_header_(menu_header ? AddChildView(std::move(menu_header))
                               : nullptr),
      options_container_(AddChildView(std::make_unique<views::View>())) {
  DCHECK(delegate_);
  options_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, inside_border_insets,
      kSpaceBetweenMenuItem));
}

CaptureModeOption* CaptureModeMenuGroup::GetOptionById(int option_id) const {
  auto iter = base::ranges::find(options_, option_id, &CaptureModeOption::id);
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

views::View* CaptureModeMenuGroup::menu_header() const {
  return menu_header_;
}

BEGIN_METADATA(CaptureModeMenuGroup)
END_METADATA

}  // namespace ash
