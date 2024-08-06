// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_list_view.h"

#include <memory>

#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kMinFontSizeDelta = -10;

// Represents a row in the scrollable IME list; each row is either an IME or
// an IME property. A checkmark icon is shown in the row if selected.
class ImeListItemView : public views::Button {
  METADATA_HEADER(ImeListItemView, views::Button)

 public:
  ImeListItemView(ImeListView* list_view,
                  const std::u16string& id,
                  const std::u16string& label,
                  bool selected,
                  const ui::ColorId button_color_id)
      : ime_list_view_(list_view) {
    SetCallback(base::BindRepeating(&ImeListItemView::PerformAction,
                                    base::Unretained(this)));
    TrayPopupUtils::ConfigureRowButtonInkdrop(views::InkDrop::Get(this));
    SetHasInkDropActionOnClick(true);

    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
        /*use_wide_layout=*/true);
    AddChildView(tri_view);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // |id_label| contains the IME short name (e.g., 'US', 'GB', 'IT').
    views::Label* id_label = TrayPopupUtils::CreateDefaultLabel();
    id_label->SetEnabledColorId(
        static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface));
    id_label->SetAutoColorReadabilityEnabled(false);
    id_label->SetText(id);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& base_font_list =
        rb.GetFontList(ui::ResourceBundle::MediumBoldFont);
    id_label->SetFontList(base_font_list);

    // IMEs having a short name of more than two characters (e.g., 'INTL') will
    // elide if rendered within |kMenuIconSize|. Shrink the font size until the
    // entire short name fits within the bounds.
    int size_delta = -1;
    while ((id_label->GetPreferredSize().width() -
            id_label->GetInsets().width()) > kMenuIconSize &&
           size_delta >= kMinFontSizeDelta) {
      id_label->SetFontList(base_font_list.DeriveWithSizeDelta(size_delta));
      --size_delta;
    }
    tri_view->AddView(TriView::Container::START, id_label);

    // The label shows the IME full name.
    auto* label_view = TrayPopupUtils::CreateDefaultLabel();
    label_view->SetText(label);
    label_view->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label_view);
    label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, label_view);

    if (selected) {
      // The checked button indicates the IME is selected.
      views::ImageView* checked_image =
          TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/true);
      checked_image->SetImage(ui::ImageModel::FromVectorIcon(
          kHollowCheckCircleIcon, button_color_id, kMenuIconSize));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    GetViewAccessibility().SetName(label_view->GetText());
    GetViewAccessibility().SetRole(ax::mojom::Role::kCheckBox);
    GetViewAccessibility().SetCheckedState(
        selected ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
  }
  ImeListItemView(const ImeListItemView&) = delete;
  ImeListItemView& operator=(const ImeListItemView&) = delete;
  ~ImeListItemView() override = default;

  // views::Button:
  void OnFocus() override {
    views::Button::OnFocus();
    if (ime_list_view_) {
      ime_list_view_->ScrollItemToVisible(this);
    }
  }

  void PerformAction(const ui::Event& event) {
    ime_list_view_->set_last_item_selected_with_keyboard(
        ime_list_view_->should_focus_ime_after_selection_with_keyboard() &&
        event.type() == ui::EventType::kKeyPressed);
    ime_list_view_->HandleViewClicked(this);
  }

 private:
  raw_ptr<ImeListView> ime_list_view_;
};

BEGIN_METADATA(ImeListItemView)
END_METADATA

}  // namespace

// Contains a toggle button to let the user enable/disable whether the
// on-screen keyboard should be shown when focusing a textfield. This row is
// shown only under certain conditions, e.g., when an external keyboard is
// attached and the user is in TabletMode mode.
class KeyboardStatusRow : public views::View {
  METADATA_HEADER(KeyboardStatusRow, views::View)

 public:
  KeyboardStatusRow() = default;
  KeyboardStatusRow(const KeyboardStatusRow&) = delete;
  KeyboardStatusRow& operator=(const KeyboardStatusRow&) = delete;
  ~KeyboardStatusRow() override = default;

  views::ToggleButton* toggle() const { return toggle_; }
  Switch* qs_toggle() const { return qs_toggle_; }

  void Init(views::Button::PressedCallback callback) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
        /*use_wide_layout=*/true);
    AddChildView(tri_view);

    // The on-screen keyboard image button.
    views::ImageView* keyboard_image =
        TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/true);
    keyboard_image->SetImage(ui::ImageModel::FromVectorIcon(
        kImeMenuOnScreenKeyboardIcon,
        static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface),
        kMenuIconSize));
    tri_view->AddView(TriView::Container::START, keyboard_image);

    // The on-screen keyboard label ('On-screen keyboard').
    auto* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
    label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label);
    tri_view->AddView(TriView::Container::CENTER, label);

    // The on-screen keyboard toggle button.
    auto qs_toggle = std::make_unique<Switch>(std::move(callback));
    qs_toggle->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
    qs_toggle->SetIsOn(keyboard::IsKeyboardEnabled());
    qs_toggle_ = qs_toggle.release();
    tri_view->AddView(TriView::Container::END, qs_toggle_);
    tri_view->SetInsets(gfx::Insets::TLBR(0, 24, 0, 12));
  }

 private:
  // `ToggleButton` to toggle keyboard on or off.
  raw_ptr<views::ToggleButton> toggle_ = nullptr;

  // `KnobSwitch` to toggle keyboard on or off.
  raw_ptr<Switch> qs_toggle_ = nullptr;
};

BEGIN_METADATA(KeyboardStatusRow)
END_METADATA

ImeListView::ImeListView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {}

ImeListView::~ImeListView() = default;

void ImeListView::Init(bool show_keyboard_toggle,
                       SingleImeBehavior single_ime_behavior) {
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  Update(ime_controller->current_ime().id, ime_controller->GetVisibleImes(),
         ime_controller->current_ime_menu_items(), show_keyboard_toggle,
         single_ime_behavior);

  scroller()->SetID(VIEW_ID_IME_LIST_VIEW_SCROLLER);
}

void ImeListView::Update(const std::string& current_ime_id,
                         const std::vector<ImeInfo>& list,
                         const std::vector<ImeMenuItem>& property_items,
                         bool show_keyboard_toggle,
                         SingleImeBehavior single_ime_behavior) {
  ResetImeListView();
  ime_map_.clear();
  property_map_.clear();
  CreateScrollableList();

  // Setup the container for the IME list views.
  container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());

  if (single_ime_behavior == ImeListView::SHOW_SINGLE_IME || list.size() > 1) {
    AppendImeListAndProperties(current_ime_id, list, property_items);
  }

  if (show_keyboard_toggle) {
    PrependKeyboardStatusRow();
  }

  DeprecatedLayoutImmediately();
  SchedulePaint();

  if (should_focus_ime_after_selection_with_keyboard_ &&
      last_item_selected_with_keyboard_) {
    FocusCurrentImeIfNeeded();
  } else if (current_ime_view_) {
    ScrollItemToVisible(current_ime_view_);
  }
}

void ImeListView::ResetImeListView() {
  // Children are removed from the view hierarchy and deleted in Reset().
  Reset();
  keyboard_status_row_ = nullptr;
  current_ime_view_ = nullptr;
  container_ = nullptr;
}

void ImeListView::ScrollItemToVisible(views::View* item_view) {
  if (scroll_content()) {
    scroll_content()->ScrollRectToVisible(item_view->bounds());
  }
}

void ImeListView::CloseImeListView() {
  last_selected_item_id_.clear();
  current_ime_view_ = nullptr;
  last_item_selected_with_keyboard_ = false;
  GetWidget()->Close();
}

void ImeListView::AppendImeListAndProperties(
    const std::string& current_ime_id,
    const std::vector<ImeInfo>& list,
    const std::vector<ImeMenuItem>& property_list) {
  DCHECK(ime_map_.empty());
  DCHECK(container_);

  for (size_t i = 0; i < list.size(); i++) {
    const bool selected = current_ime_id == list[i].id;
    views::View* ime_view =
        container_->AddChildView(std::make_unique<ImeListItemView>(
            this, list[i].short_name, list[i].name, selected,
            static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)));
    ime_map_[ime_view] = list[i].id;

    if (selected) {
      current_ime_view_ = ime_view;
    }

    // Add the properties, if any, of the currently-selected IME.
    if (selected && !property_list.empty()) {
      // Adds a separator on the top of property items.
      container_->AddChildView(TrayPopupUtils::CreateListItemSeparator(true));

      // Adds the property items.
      for (const auto& property : property_list) {
        ImeListItemView* property_view =
            container_->AddChildView(std::make_unique<ImeListItemView>(
                this, std::u16string(), property.label, property.checked,
                static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)));

        property_map_[property_view] = property.key;
      }

      // Adds a separator on the bottom of property items if there are still
      // other IMEs under the current one.
      if (i < list.size() - 1) {
        container_->AddChildView(TrayPopupUtils::CreateListItemSeparator(true));
      }
    }
  }
}

void ImeListView::PrependKeyboardStatusRow() {
  DCHECK(!keyboard_status_row_);
  keyboard_status_row_ = new KeyboardStatusRow;
  keyboard_status_row_->Init(base::BindRepeating(
      &ImeListView::KeyboardStatusTogglePressed, base::Unretained(this)));
  container_->AddChildViewAt(keyboard_status_row_.get(), 0);
}

void ImeListView::KeyboardStatusTogglePressed() {
  Shell::Get()
      ->keyboard_controller()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
  last_selected_item_id_.clear();
  last_item_selected_with_keyboard_ = false;
}

void ImeListView::HandleViewClicked(views::View* view) {
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  std::map<views::View*, std::string>::const_iterator ime = ime_map_.find(view);
  if (ime != ime_map_.end()) {
    base::RecordAction(base::UserMetricsAction("StatusArea_IME_SwitchMode"));
    std::string ime_id = ime->second;
    last_selected_item_id_ = ime_id;
    ime_controller->SwitchImeById(ime_id, false /* show_message */);
    UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeSwitch", ImeSwitchType::kTray);

  } else {
    std::map<views::View*, std::string>::const_iterator property =
        property_map_.find(view);
    if (property == property_map_.end()) {
      return;
    }
    const std::string key = property->second;
    last_selected_item_id_ = key;
    ime_controller->ActivateImeMenuItem(key);
  }

  if (!should_focus_ime_after_selection_with_keyboard_ ||
      !last_item_selected_with_keyboard_) {
    CloseImeListView();
  }
}

void ImeListView::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible ||
      (should_focus_ime_after_selection_with_keyboard_ &&
       last_item_selected_with_keyboard_) ||
      !current_ime_view_) {
    return;
  }

  ScrollItemToVisible(current_ime_view_);
}

void ImeListView::FocusCurrentImeIfNeeded() {
  views::FocusManager* manager = GetFocusManager();
  if (!manager || manager->GetFocusedView() || last_selected_item_id_.empty()) {
    return;
  }

  for (auto ime_map : ime_map_) {
    if (ime_map.second == last_selected_item_id_) {
      (ime_map.first)->RequestFocus();
      return;
    }
  }

  for (auto property_map : property_map_) {
    if (property_map.second == last_selected_item_id_) {
      (property_map.first)->RequestFocus();
      return;
    }
  }
}

BEGIN_METADATA(ImeListView)
END_METADATA

ImeListViewTestApi::ImeListViewTestApi(ImeListView* ime_list_view)
    : ime_list_view_(ime_list_view) {}

ImeListViewTestApi::~ImeListViewTestApi() = default;

views::View* ImeListViewTestApi::GetToggleView() const {
  if (!ime_list_view_->keyboard_status_row_) {
    return nullptr;
  }
  return ime_list_view_->keyboard_status_row_->qs_toggle();
}

}  // namespace ash
