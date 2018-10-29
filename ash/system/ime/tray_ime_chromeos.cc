// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime_chromeos.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/tray_accessibility.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item_detailed_view_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
class IMETrayItemView : public TrayItemView, public SessionObserver {
 public:
  explicit IMETrayItemView(SystemTrayItem* owner) : TrayItemView(owner) {
    CreateLabel();
    SetupLabelForTray(label());
  }
  ~IMETrayItemView() override = default;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override {
    label()->SetEnabledColor(TrayIconColor(state));
  }

 private:
  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(IMETrayItemView);
};
}  // namespace

namespace tray {

class IMEDefaultView : public TrayItemMore {
 public:
  IMEDefaultView(SystemTrayItem* owner, const base::string16& label)
      : TrayItemMore(owner) {
    SetImage(gfx::CreateVectorIcon(kSystemMenuKeyboardIcon, kMenuIconColor));
    UpdateLabel(label);
  }

  ~IMEDefaultView() override = default;

  void UpdateLabel(const base::string16& label) {
    SetLabel(label);
    SetAccessibleName(label);
  }

 protected:
  // TrayItemMore:
  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();

    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(
        gfx::CreateVectorIcon(kSystemMenuKeyboardIcon, style->GetIconColor()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEDefaultView);
};

IMEDetailedView::IMEDetailedView(DetailedViewDelegate* delegate,
                                 ImeController* ime_controller)
    : ImeListView(delegate), ime_controller_(ime_controller) {
  DCHECK(ime_controller_);
}

void IMEDetailedView::Update(
    const std::string& current_ime_id,
    const std::vector<mojom::ImeInfo>& list,
    const std::vector<mojom::ImeMenuItem>& property_list,
    bool show_keyboard_toggle,
    SingleImeBehavior single_ime_behavior) {
  ImeListView::Update(current_ime_id, list, property_list, show_keyboard_toggle,
                      single_ime_behavior);
  CreateTitleRow(IDS_ASH_STATUS_TRAY_IME);
}

void IMEDetailedView::ResetImeListView() {
  ImeListView::ResetImeListView();
  settings_button_ = nullptr;
  controlled_setting_icon_ = nullptr;
}

void IMEDetailedView::HandleButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == settings_button_)
    ShowSettings();
  else
    ImeListView::HandleButtonPressed(sender, event);
}

void IMEDetailedView::CreateExtraTitleRowButtons() {
  if (ime_controller_->managed_by_policy()) {
    controlled_setting_icon_ = TrayPopupUtils::CreateMainImageView();
    controlled_setting_icon_->SetImage(
        gfx::CreateVectorIcon(kSystemMenuBusinessIcon, kMenuIconColor));
    controlled_setting_icon_->set_tooltip_text(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME_MANAGED));
    tri_view()->AddView(TriView::Container::END, controlled_setting_icon_);
  }

  tri_view()->SetContainerVisible(TriView::Container::END, true);
  settings_button_ = CreateSettingsButton(IDS_ASH_STATUS_TRAY_IME_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void IMEDetailedView::ShowSettings() {
  base::RecordAction(base::UserMetricsAction("StatusArea_IME_Detailed"));
  Shell::Get()->system_tray_model()->client_ptr()->ShowIMESettings();
  CloseBubble();
}

}  // namespace tray

TrayIME::TrayIME(SystemTray* system_tray)
    : SystemTrayItem(system_tray, SystemTrayItemUmaType::UMA_IME),
      ime_controller_(Shell::Get()->ime_controller()),
      tray_label_(nullptr),
      default_(nullptr),
      detailed_(nullptr),
      keyboard_suppressed_(false),
      is_visible_(true),
      detailed_view_delegate_(
          std::make_unique<SystemTrayItemDetailedViewDelegate>(this)) {
  DCHECK(ime_controller_);
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->AddVirtualKeyboardObserver(this);
  tray_notifier->AddIMEObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

TrayIME::~TrayIME() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->RemoveIMEObserver(this);
  tray_notifier->RemoveVirtualKeyboardObserver(this);
}

void TrayIME::OnKeyboardSuppressionChanged(bool suppressed) {
  keyboard_suppressed_ = suppressed;
  Update();
}

void TrayIME::OnAccessibilityStatusChanged() {
  Update();
}

void TrayIME::Update() {
  size_t ime_count = ime_controller_->available_imes().size();
  UpdateTrayLabel(ime_controller_->current_ime(), ime_count);
  if (default_) {
    default_->SetVisible(ShouldDefaultViewBeVisible());
    default_->UpdateLabel(GetDefaultViewLabel(ime_count > 1));
  }
  if (detailed_) {
    detailed_->Update(ime_controller_->current_ime().id,
                      ime_controller_->available_imes(),
                      ime_controller_->current_ime_menu_items(),
                      ShouldShowKeyboardToggle(), GetSingleImeBehavior());
  }
}

void TrayIME::UpdateTrayLabel(const mojom::ImeInfo& current, size_t count) {
  if (tray_label_) {
    bool visible = ShouldShowImeTrayItem(count) && is_visible_;
    tray_label_->SetVisible(visible);
    // Do not change label before hiding because this change is noticeable.
    if (!visible)
      return;
    if (current.third_party) {
      tray_label_->label()->SetText(current.short_name +
                                    base::UTF8ToUTF16("*"));
    } else {
      tray_label_->label()->SetText(current.short_name);
    }
    tray_label_->Layout();
  }
}

bool TrayIME::ShouldShowKeyboardToggle() {
  return keyboard_suppressed_ &&
         !Shell::Get()->accessibility_controller()->IsVirtualKeyboardEnabled();
}

base::string16 TrayIME::GetDefaultViewLabel(bool show_ime_label) {
  if (show_ime_label) {
    return ime_controller_->current_ime().name;
  } else {
    // Display virtual keyboard status instead.
    int id = keyboard::IsKeyboardEnabled()
                 ? IDS_ASH_STATUS_TRAY_KEYBOARD_ENABLED
                 : IDS_ASH_STATUS_TRAY_KEYBOARD_DISABLED;
    return l10n_util::GetStringUTF16(id);
  }
}

views::View* TrayIME::CreateTrayView(LoginStatus status) {
  CHECK(tray_label_ == nullptr);
  tray_label_ = new IMETrayItemView(this);
  // Hide IME tray when it is created, it will be updated when it is notified
  // of the IME refresh event.
  tray_label_->SetVisible(false);
  return tray_label_;
}

views::View* TrayIME::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == nullptr);
  default_ = new tray::IMEDefaultView(
      this, GetDefaultViewLabel(ShouldShowImeTrayItem(
                ime_controller_->available_imes().size())));
  default_->SetVisible(ShouldDefaultViewBeVisible());
  return default_;
}

views::View* TrayIME::CreateDetailedView(LoginStatus status) {
  CHECK(detailed_ == nullptr);
  detailed_ =
      new tray::IMEDetailedView(detailed_view_delegate_.get(), ime_controller_);
  detailed_->Init(ShouldShowKeyboardToggle(), GetSingleImeBehavior());
  return detailed_;
}

void TrayIME::OnTrayViewDestroyed() {
  tray_label_ = nullptr;
}

void TrayIME::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

void TrayIME::OnDetailedViewDestroyed() {
  detailed_ = nullptr;
}

void TrayIME::OnIMERefresh() {
  Update();
}

void TrayIME::OnIMEMenuActivationChanged(bool is_active) {
  is_visible_ = !is_active;
  Update();
}

bool TrayIME::IsIMEManaged() {
  return ime_controller_->managed_by_policy();
}

bool TrayIME::ShouldDefaultViewBeVisible() {
  return is_visible_ &&
         (ShouldShowImeTrayItem(ime_controller_->available_imes().size()) ||
          ime_controller_->current_ime_menu_items().size() > 1 ||
          ShouldShowKeyboardToggle());
}

bool TrayIME::ShouldShowImeTrayItem(size_t ime_count) {
  // If managed, we want to show the tray icon even if there's only one input
  // method to choose from.
  size_t threshold = IsIMEManaged() ? 1 : 2;
  return ime_count >= threshold;
}

ImeListView::SingleImeBehavior TrayIME::GetSingleImeBehavior() {
  // If managed, we also want to show a single IME.
  return IsIMEManaged() ? ImeListView::SHOW_SINGLE_IME
                        : ImeListView::HIDE_SINGLE_IME;
}

views::View* TrayIME::GetControlledSettingIconForTesting() {
  return detailed_ ? detailed_->controlled_setting_icon() : nullptr;
}

}  // namespace ash
