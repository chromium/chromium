// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_

#include <memory>
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace quick_pair {

using DiscoveryCallback = base::OnceCallback<void(DiscoveryAction)>;
using PairingFailedCallback = base::OnceCallback<void(PairingFailedAction)>;
using AssociateAccountCallback =
    base::OnceCallback<void(AssociateAccountAction)>;
using CompanionAppCallback = base::OnceCallback<void(CompanionAppAction)>;

class FastPairPresenter {
 public:
  FastPairPresenter();
  FastPairPresenter(const FastPairPresenter&) = delete;
  FastPairPresenter& operator=(const FastPairPresenter&) = delete;
  ~FastPairPresenter();

  void ShowDiscovery(const Device& device, DiscoveryCallback callback);
  void ShowPairing(const Device& device);
  void ShowPairingFailed(const Device& device, PairingFailedCallback callback);
  void ShowAssociateAccount(const Device& device,
                            AssociateAccountCallback callback);
  void ShowCompanionApp(const Device& device, CompanionAppCallback callback);

 private:
  void OnDiscoveryClicked(DiscoveryCallback action_callback);
  void OnDiscoveryDismissed(DiscoveryCallback callback, bool user_dismissed);

  void OnNavigateToSettings(PairingFailedCallback callback);
  void OnPairingFailedDismissed(PairingFailedCallback callback,
                                bool user_dismissed);

  std::unique_ptr<FastPairNotificationController> notification_controller_;
  base::WeakPtrFactory<FastPairPresenter> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
