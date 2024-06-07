// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"

namespace ash {

MagicBoostControllerAsh::MagicBoostControllerAsh() = default;

MagicBoostControllerAsh::~MagicBoostControllerAsh() = default;

void MagicBoostControllerAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::MagicBoostController> receiver) {
  // The receiver is only from lacros chrome as present, but more mojo clients
  // may be added in the future.
  receivers_.Add(this, std::move(receiver));
}

void MagicBoostControllerAsh::ShowDisclaimerUi(
    int64_t display_id,
    crosapi::mojom::MagicBoostController::TransitionAction action) {
  if (disclaimer_widget_) {
    return;
  }

  // TODO(b/341832244): Pass in the correct callback to set the feature state.
  disclaimer_widget_ = MagicBoostDisclaimerView::CreateWidget(
      display_id,
      /*press_accept_button_callback*/ base::DoNothing(),
      /*press_decline_button_callback*/ base::DoNothing());
  disclaimer_widget_->Show();
}

}  // namespace ash
