// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_notifier_settings_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/notifier_settings_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

class UnifiedNotifierSettingsView
    : public TrayDetailedView,
      public message_center::MessageCenterObserver {
 public:
  explicit UnifiedNotifierSettingsView(DetailedViewDelegate* delegate)
      : TrayDetailedView(delegate), settings_view_(new NotifierSettingsView()) {
    CreateTitleRow(IDS_ASH_MESSAGE_CENTER_FOOTER_TITLE);
    AddChildView(settings_view_);
    box_layout()->SetFlexForView(settings_view_, 1);
    OnQuietModeChanged(message_center::MessageCenter::Get()->IsQuietMode());
    message_center::MessageCenter::Get()->AddObserver(this);
  }

  ~UnifiedNotifierSettingsView() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  // MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override {
    settings_view_->SetQuietModeState(in_quiet_mode);
  }

  const char* GetClassName() const override {
    return "UnifiedNotifierSettingsView";
  }

 private:
  NotifierSettingsView* const settings_view_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedNotifierSettingsView);
};

}  // namespace

UnifiedNotifierSettingsController::UnifiedNotifierSettingsController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedNotifierSettingsController::~UnifiedNotifierSettingsController() =
    default;

views::View* UnifiedNotifierSettingsController::CreateView() {
  return new UnifiedNotifierSettingsView(detailed_view_delegate_.get());
}

}  // namespace ash
