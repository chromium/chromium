// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_OPERATION_H_
#define ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_OPERATION_H_

#include "ash/components/tether/message_transfer_operation.h"
#include "base/time/time.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/time/clock.h"

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace tether {

// Operation which sends a disconnect message to a tether host.
class DisconnectTetheringOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<DisconnectTetheringOperation> Create(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DisconnectTetheringOperation> CreateInstance(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client) = 0;

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    // Alerts observers when the operation has finished for device with ID
    // |device_id|. |success| is true when the operation successfully sends the
    // message and false otherwise.
    virtual void OnOperationFinished(const std::string& device_id,
                                     bool success) = 0;
  };

  DisconnectTetheringOperation(const DisconnectTetheringOperation&) = delete;
  DisconnectTetheringOperation& operator=(const DisconnectTetheringOperation&) =
      delete;

  ~DisconnectTetheringOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  DisconnectTetheringOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  void NotifyObserversOperationFinished(bool success);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;
  void OnMessageSent(int sequence_number) override;

 private:
  friend class DisconnectTetheringOperationTest;
  FRIEND_TEST_ALL_PREFIXES(DisconnectTetheringOperationTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(DisconnectTetheringOperationTest, TestFailure);
  FRIEND_TEST_ALL_PREFIXES(DisconnectTetheringOperationTest,
                           DisconnectRequestSentOnceAuthenticated);

  void SetClockForTest(base::Clock* clock_for_test);

  base::ObserverList<Observer>::Unchecked observer_list_;
  multidevice::RemoteDeviceRef remote_device_;
  int disconnect_message_sequence_number_ = -1;
  bool has_sent_message_;

  base::Clock* clock_;
  base::Time disconnect_start_time_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_OPERATION_H_
