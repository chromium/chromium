// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
#define ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_

#include <memory>
#include <optional>

#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {
namespace quick_pair {

class Device;
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
  void ShowInstallCompanionApp(scoped_refptr<Device> device,
                               CompanionAppCallback callback) override;
  void ShowLaunchCompanionApp(scoped_refptr<Device> device,
                              CompanionAppCallback callback) override;
  void ShowPasskey(std::u16string device_name, uint32_t passkey) override;
  void RemoveNotifications() override;
  void ExtendNotification() override;

 private:
  FastPairPresenterImpl(const FastPairPresenterImpl&) = delete;
  FastPairPresenterImpl& operator=(const FastPairPresenterImpl&) = delete;

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
                            FastPairNotificationDismissReason dismiss_reason);
  void OnDiscoveryLearnMoreClicked(DiscoveryCallback action_callback);

  void OnNavigateToSettings(PairingFailedCallback callback);
  void OnPairingFailedDismissed(
      PairingFailedCallback callback,
      FastPairNotificationDismissReason dismiss_reason);

  void OnAssociateAccountActionClicked(AssociateAccountCallback callback);
  void OnAssociateAccountLearnMoreClicked(AssociateAccountCallback callback);
  void OnAssociateAccountDismissed(
      AssociateAccountCallback callback,
      FastPairNotificationDismissReason dismiss_reason);

  void OnCompanionAppInstallClicked(CompanionAppCallback callback);
  void OnCompanionAppSetupClicked(CompanionAppCallback callback);
  void OnCompanionAppDismissed(
      CompanionAppCallback callback,
      FastPairNotificationDismissReason dismiss_reason);

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
  void OnInstallCompanionAppMetadataRetrieved(scoped_refptr<Device> device,
                                              CompanionAppCallback callback,
                                              DeviceMetadata* device_metadata,
                                              bool has_retryable_error);
  void OnLaunchCompanionAppMetadataRetrieved(scoped_refptr<Device> device,
                                             CompanionAppCallback callback,
                                             DeviceMetadata* device_metadata,
                                             bool has_retryable_error);

  // TODO(b/274973687): remove once notification replaces Bluetooth connect
  // toast A timer used to delay displaying the Fast Pair companion app
  // notification until the Bluetooth device-connected toast is almost or
  // already auto-dismissed. Only one Fast Pair companion notification is
  // supported at a time. If a new device with companion app is paired before
  // the previous notification shows, only the new device's companion
  // notification will be shown.
  base::OneShotTimer toast_collision_avoidance_timer_;
  // TODO(b/274973687): remove once notification replaces Bluetooth connect
  // toast
  void ShowInstallCompanionAppDelayed(scoped_refptr<Device> device,
                                      CompanionAppCallback callback);
  // TODO(b/274973687): remove once notification replaces Bluetooth connect
  // toast
  void ShowLaunchCompanionAppDelayed(scoped_refptr<Device> device,
                                     CompanionAppCallback callback);

  std::unique_ptr<FastPairNotificationController> notification_controller_;
  base::WeakPtrFactory<FastPairPresenterImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
