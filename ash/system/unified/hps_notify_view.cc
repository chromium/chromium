// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_view.h"

#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

HpsNotifyView::HpsNotifyView(Shelf* shelf)
    : TrayItemView(shelf),
      hps_state_(false),
      is_oobe_(false),
      first_signal_received_(false),
      session_observation_(this),
      hps_dbus_observation_(this),
      weak_ptr_factory_(this) {
  CreateImageView();
  UpdateIconColor(Shell::Get()->session_controller()->GetSessionState());
  SetVisible(false);

  session_observation_.Observe(Shell::Get()->session_controller());
  hps_dbus_observation_.Observe(chromeos::HpsDBusClient::Get());

  chromeos::HpsDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
      &HpsNotifyView::OnHpsPollResult, weak_ptr_factory_.GetWeakPtr()));
}

HpsNotifyView::~HpsNotifyView() = default;

void HpsNotifyView::HandleLocaleChange() {}

void HpsNotifyView::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  UpdateIconColor(session_state);
  UpdateIconVisibility(session_state == session_manager::SessionState::OOBE,
                       hps_state_);
}

void HpsNotifyView::OnHpsNotifyChanged(bool hps_state) {
  first_signal_received_ = true;
  UpdateIconVisibility(is_oobe_, hps_state);
}

void HpsNotifyView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIconColor(Shell::Get()->session_controller()->GetSessionState());
}

const char* HpsNotifyView::GetClassName() const {
  return "HpsNotifyView";
}

void HpsNotifyView::UpdateIconColor(
    session_manager::SessionState session_state) {
  const SkColor new_color = TrayIconColor(session_state);
  const gfx::ImageSkia new_icon = gfx::CreateVectorIcon(gfx::IconDescription(
      kSystemTrayHpsNotifyIcon, kUnifiedTrayIconSize, new_color));
  image_view()->SetImage(new_icon);
}

void HpsNotifyView::UpdateIconVisibility(bool is_oobe, bool hps_state) {
  if (is_oobe_ == is_oobe && hps_state_ == hps_state)
    return;

  is_oobe_ = is_oobe;
  hps_state_ = hps_state;
  SetVisible(!is_oobe_ && hps_state_);
}

void HpsNotifyView::OnHpsPollResult(absl::optional<bool> result) {
  // A race: we have received a signal before we had a chance to establish our
  // initial state (from the result of our DBus method call). The signal could
  // have originated from either before our DBus method call, or after.
  //
  // If the two states are the same, there is no problem. If they are different,
  // we should always ignore the DBus method response because:
  //   a) If the signal originated before the DBus method call, the state of the
  //      daemon has changed since the signal was sent and an updated signal
  //      will be forthcoming.
  //   b) If the signal originated after the DBus method call, then the signal
  //      is newer and reflects the current state of the daemon.
  if (first_signal_received_ || !result.has_value()) {
    return;
  }

  UpdateIconVisibility(is_oobe_, *result);
}

}  // namespace ash
