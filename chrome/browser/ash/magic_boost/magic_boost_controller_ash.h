// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_ASH_H_

#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// `MagicBoostControllerAsh` is the central point to deal with the ChromeOS -
// Chrome browser communication. it is responsible for showing the disclaimer UI
// and connect with Orca services in ash.
class MagicBoostControllerAsh : public crosapi::mojom::MagicBoostController {
 public:
  MagicBoostControllerAsh();

  MagicBoostControllerAsh(const MagicBoostControllerAsh&) = delete;
  MagicBoostControllerAsh& operator=(const MagicBoostControllerAsh&) = delete;

  ~MagicBoostControllerAsh() override;

  // Binds a pending receiver connected to a lacros mojo client to the delegate.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::MagicBoostController> receiver);

  // crosapi::mojom::MagicBoostController:
  void ShowDisclaimerUi(
      int64_t display_id,
      crosapi::mojom::MagicBoostController::TransitionAction action) override;

 private:
  mojo::ReceiverSet<crosapi::mojom::MagicBoostController> receivers_;

  views::UniqueWidgetPtr disclaimer_widget_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_ASH_H_
