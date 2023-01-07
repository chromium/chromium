// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_

#include <memory>

#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

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

class FastPairPresenterImpl : public FastPairPresenter {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairPresenter> Create(
        message_center::MessageCenter* message_center);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairPresenter> CreateInstance(
        message_center::MessageCenter* message_center) = 0;

   private:
    static Factory* g_test_factory_;
  };

  explicit FastPairPresenterImpl(message_center::MessageCenter* message_center);

  ~FastPairPresenterImpl() override;

  void ShowDiscovery(scoped_refptr<Device> device,
                     DiscoveryCallback callback) override;
  void ShowPairing(scoped_refptr<Device> device) override;
  void ShowPairingFailed(scoped_refptr<Device> device,
                         PairingFailedCallback callback) override;
  void ShowAssociateAccount(scoped_refptr<Device> device,
                            AssociateAccountCallback callback) override;
  void ShowCompanionApp(scoped_refptr<Device> device,
                        CompanionAppCallback callback) override;
  void RemoveNotifications(
      bool clear_already_shown_discovery_notification_cache) override;
  void RemoveDeviceFromAlreadyShownDiscoveryNotificationCache(
      scoped_refptr<Device> device) override;

  // When a device is lost, prevent notifications for it for a timeout.
  // This will allow devices that are lost to appear again for a user without
  // toggling Fast Pair scanning. This prevents a case where a device cycles
  // through found->lost->found, and the notifications appear and reappear.
  void StartDeviceLostTimer(scoped_refptr<Device> device) override;

 private:
  FastPairPresenterImpl(const FastPairPresenterImpl&) = delete;
  FastPairPresenterImpl& operator=(const FastPairPresenterImpl&) = delete;

  // Object representing devices we have already shown notifications for. We
  // use `DevicesWithDiscoveryNotificationAlreadyShown` in order to prevent
  // storing `Device` objects whose lifetime might have ended. We store them
  // in the |address_to_devices_with_discovery_notification_already_shown_map_|
  // map using the device's |ble_address| as the key.
  struct DevicesWithDiscoveryNotificationAlreadyShown {
    Protocol protocol;
    std::string metadata_id;
  };

  void OnCheckOptInStatus(scoped_refptr<Device> device,
                          DiscoveryCallback callback,
                          DeviceMetadata* device_metadata,
                          nearby::fastpair::OptInStatus status);

  void ShowUserDiscoveryNotification(scoped_refptr<Device> device,
                                     DiscoveryCallback callback,
                                     DeviceMetadata* device_metadata);
  void ShowGuestDiscoveryNotification(scoped_refptr<Device> device,
                                      DiscoveryCallback callback,
                                      DeviceMetadata* device_metadata);
  void ShowSubsequentDiscoveryNotification(scoped_refptr<Device> device,
                                           DiscoveryCallback callback,
                                           DeviceMetadata* device_metadata);
  void OnDiscoveryClicked(DiscoveryCallback action_callback);
  void OnDiscoveryDismissed(scoped_refptr<Device> device,
                            DiscoveryCallback callback,
                            bool user_dismissed);
  void OnDiscoveryLearnMoreClicked(DiscoveryCallback action_callback);
  bool WasDiscoveryNotificationAlreadyShownForDevice(const Device& device);
  void AddDeviceToDiscoveryNotificationAlreadyShownMap(
      scoped_refptr<Device> device);
  void AllowNotificationForRecentlyLostDevice(scoped_refptr<Device> device);

  void OnNavigateToSettings(PairingFailedCallback callback);
  void OnPairingFailedDismissed(PairingFailedCallback callback,
                                bool user_dismissed);

  void OnAssociateAccountActionClicked(AssociateAccountCallback callback);
  void OnAssociateAccountLearnMoreClicked(AssociateAccountCallback callback);
  void OnAssociateAccountDismissed(AssociateAccountCallback callback,
                                   bool user_dismissed);

  void OnDiscoveryMetadataRetrieved(scoped_refptr<Device> device,
                                    DiscoveryCallback callback,
                                    DeviceMetadata* device_metadata,
                                    bool has_retryable_error);
  void OnPairingMetadataRetrieved(scoped_refptr<Device> device,
                                  DeviceMetadata* device_metadata,
                                  bool has_retryable_error);
  void OnPairingFailedMetadataRetrieved(scoped_refptr<Device> device,
                                        PairingFailedCallback callback,
                                        DeviceMetadata* device_metadata,
                                        bool has_retryable_error);
  void OnAssociateAccountMetadataRetrieved(scoped_refptr<Device> device,
                                           AssociateAccountCallback callback,
                                           DeviceMetadata* device_metadata,
                                           bool has_retryable_error);

  // Store the device we are currently displaying a discovery notification
  // for using |ble_address| as key. In the Fast Pair flow, it is possible for a
  // discovery notification to repeatedly appear for some devices, especially in
  // the case of Subsequent Pairing when we are parsing multiple advertisements
  // and finding a match each time. We only need this check for Discovery
  // Notifications since the Error Notification and Associate Account
  // Notification are triggered once per device action (e.g., pairing failed,
  // classic Bluetooth pairing). This logic is required to avoid repeatedly
  // showing and dismissing a notification.
  base::flat_map<std::string, DevicesWithDiscoveryNotificationAlreadyShown>
      address_to_devices_with_discovery_notification_already_shown_map_;

  // Keep track of timers for each lost device that will fire to remove the
  // device from
  // |address_to_devices_with_discovery_notification_already_shown_map_| and
  // allow the notification to be shown again. The key is the device's
  // ble address that matches the key in
  // |address_to_devices_with_discovery_notification_already_shown_map_|.
  std::map<std::string, std::unique_ptr<base::OneShotTimer>>
      address_to_lost_device_timer_map_;

  std::unique_ptr<FastPairNotificationController> notification_controller_;
  base::WeakPtrFactory<FastPairPresenterImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
