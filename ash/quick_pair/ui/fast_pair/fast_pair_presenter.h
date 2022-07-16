// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_

#include <memory>
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

struct Device;
class DeviceMetadata;

using DiscoveryCallback = base::RepeatingCallback<void(DiscoveryAction)>;
using PairingFailedCallback =
    base::RepeatingCallback<void(PairingFailedAction)>;
using AssociateAccountCallback =
    base::RepeatingCallback<void(AssociateAccountAction)>;
using CompanionAppCallback = base::RepeatingCallback<void(CompanionAppAction)>;

class FastPairPresenter {
 public:
  FastPairPresenter();
  FastPairPresenter(const FastPairPresenter&) = delete;
  FastPairPresenter& operator=(const FastPairPresenter&) = delete;
  ~FastPairPresenter();

  void ShowDiscovery(scoped_refptr<Device> device, DiscoveryCallback callback);
  void ShowPairing(scoped_refptr<Device> device);
  void ShowPairingFailed(scoped_refptr<Device> device,
                         PairingFailedCallback callback);
  void ShowAssociateAccount(scoped_refptr<Device> device,
                            AssociateAccountCallback callback);
  void ShowCompanionApp(scoped_refptr<Device> device,
                        CompanionAppCallback callback);
  void RemoveNotifications(scoped_refptr<Device> device);

 private:
  void OnDiscoveryClicked(DiscoveryCallback action_callback);
  void OnDiscoveryDismissed(DiscoveryCallback callback, bool user_dismissed);

  void OnNavigateToSettings(PairingFailedCallback callback);
  void OnPairingFailedDismissed(PairingFailedCallback callback,
                                bool user_dismissed);

  void OnAssociateAccountActionClicked(AssociateAccountCallback callback);
  void OnLearnMoreClicked(AssociateAccountCallback callback);
  void OnAssociateAccountDismissed(AssociateAccountCallback callback,
                                   bool user_dismissed);

  void OnDiscoveryMetadataRetrieved(scoped_refptr<Device> device,
                                    DiscoveryCallback callback,
                                    DeviceMetadata* device_metadata);
  void OnPairingMetadataRetrieved(scoped_refptr<Device> device,
                                  DeviceMetadata* device_metadata);
  void OnPairingFailedMetadataRetrieved(scoped_refptr<Device> device,
                                        PairingFailedCallback callback,
                                        DeviceMetadata* device_metadata);
  void OnAssociateAccountMetadataRetrieved(scoped_refptr<Device> device,
                                           AssociateAccountCallback callback,
                                           DeviceMetadata* device_metadata);

  std::unique_ptr<FastPairNotificationController> notification_controller_;
  base::WeakPtrFactory<FastPairPresenter> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
