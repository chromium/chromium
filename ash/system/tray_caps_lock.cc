// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/sys_info.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Padding used to position the caption in the caps lock default view row.
const int kCaptionRightPadding = 6;

bool IsCapsLockEnabled() {
  return Shell::Get()->ime_controller()->IsCapsLockEnabled();
}

}  // namespace

class CapsLockDefaultView : public ActionableView {
 public:
  CapsLockDefaultView()
      : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
        text_label_(TrayPopupUtils::CreateDefaultLabel()),
        shortcut_label_(TrayPopupUtils::CreateDefaultLabel()) {
    shortcut_label_->SetEnabled(false);

    TriView* tri_view(TrayPopupUtils::CreateDefaultRowView());
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(tri_view);

    auto* image = TrayPopupUtils::CreateMainImageView();
    image->SetEnabled(enabled());
    TrayPopupItemStyle default_view_style(
        TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    image->SetImage(gfx::CreateVectorIcon(kSystemMenuCapsLockIcon,
                                          default_view_style.GetIconColor()));
    default_view_style.SetupLabel(text_label_);

    TrayPopupItemStyle caption_style(TrayPopupItemStyle::FontStyle::CAPTION);
    caption_style.SetupLabel(shortcut_label_);

    SetInkDropMode(InkDropMode::ON);

    tri_view->AddView(TriView::Container::START, image);
    tri_view->AddView(TriView::Container::CENTER, text_label_);
    tri_view->AddView(TriView::Container::END, shortcut_label_);
    tri_view->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(
            0, 0, 0, kCaptionRightPadding + kTrayPopupLabelRightPadding));
  }

  ~CapsLockDefaultView() override = default;

  // Updates the label text and the shortcut text.
  void Update(bool caps_lock_enabled) {
    const int text_string_id = caps_lock_enabled
                                   ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED
                                   : IDS_ASH_STATUS_TRAY_CAPS_LOCK_DISABLED;
    text_label_->SetText(l10n_util::GetStringUTF16(text_string_id));

    int shortcut_string_id = 0;
    const bool search_mapped_to_caps_lock =
        CapsLockNotificationController::IsSearchKeyMappedToCapsLock();
    if (caps_lock_enabled) {
      shortcut_string_id =
          search_mapped_to_caps_lock
              ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH_OR_SHIFT
              : IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH_OR_SHIFT;
    } else {
      shortcut_string_id =
          search_mapped_to_caps_lock
              ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH
              : IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH;
    }
    shortcut_label_->SetText(l10n_util::GetStringUTF16(shortcut_string_id));

    Layout();
  }

 private:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kButton;
    node_data->SetName(text_label_->text());
  }

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    bool new_state = !IsCapsLockEnabled();
    Shell::Get()->ime_controller()->SetCapsLockEnabled(new_state);
    Shell::Get()->metrics()->RecordUserMetricsAction(
        new_state ? UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK
                  : UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK);
    return true;
  }

  // It indicates whether the Caps Lock is on or off.
  views::Label* text_label_;

  // It indicates the shortcut can be used to turn on or turn off Caps Lock.
  views::Label* shortcut_label_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDefaultView);
};

TrayCapsLock::TrayCapsLock(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayCapsLockIcon,
                    SystemTrayItemUmaType::UMA_CAPS_LOCK),
      default_(nullptr),
      caps_lock_enabled_(IsCapsLockEnabled()) {
  Shell::Get()->ime_controller()->AddObserver(this);
}

TrayCapsLock::~TrayCapsLock() {
  Shell::Get()->ime_controller()->RemoveObserver(this);
}

void TrayCapsLock::OnCapsLockChanged(bool enabled) {
  caps_lock_enabled_ = enabled;

  if (tray_view())
    tray_view()->SetVisible(caps_lock_enabled_);

  if (default_)
    default_->Update(caps_lock_enabled_);
}

bool TrayCapsLock::GetInitialVisibility() {
  return IsCapsLockEnabled();
}

views::View* TrayCapsLock::CreateDefaultView(LoginStatus status) {
  if (!caps_lock_enabled_)
    return nullptr;
  DCHECK(!default_);
  default_ = new CapsLockDefaultView;
  default_->Update(caps_lock_enabled_);
  return default_;
}

void TrayCapsLock::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

}  // namespace ash
