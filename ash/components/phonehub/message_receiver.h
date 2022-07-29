// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_
#define ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_

#include "ash/components/multidevice/logging/logging.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

// Responsible for receiving message updates from the remote phone device.
namespace ash {
namespace phonehub {

class MessageReceiver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the remote phone's snapshot has been updated which includes
    // phone properties and notification updates.
    virtual void OnPhoneStatusSnapshotReceived(
        proto::PhoneStatusSnapshot phone_status_snapshot) {}

    // Called when the remote phone status has been updated. Include phone
    // properties, updated notifications, and removed notifications.
    virtual void OnPhoneStatusUpdateReceived(
        proto::PhoneStatusUpdate phone_status_update) {}

    // Called when the remote feature setup is finished on the remote pohone.
    virtual void OnFeatureSetupResponseReceived(
        proto::FeatureSetupResponse feature_setup_response) {}

    // Called when the remote phone sends the list of camera roll items that
    // should be displayed via FetchCameraRollItemsResponse.
    virtual void OnFetchCameraRollItemsResponseReceived(
        const proto::FetchCameraRollItemsResponse& response) {}

    // Called when the remote phone acknowledges whether the requested camera
    // roll item file data is ready to be transferred.
    virtual void OnFetchCameraRollItemDataResponseReceived(
        const proto::FetchCameraRollItemDataResponse& response) {}
  };

  MessageReceiver(const MessageReceiver&) = delete;
  MessageReceiver& operator=(const MessageReceiver&) = delete;
  virtual ~MessageReceiver();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  MessageReceiver();

  void NotifyPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot);
  void NotifyPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update);
  void NotifyFeatureSetupResponseReceived(proto::FeatureSetupResponse response);
  void NotifyFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response);
  void NotifyFetchCameraRollItemDataResponseReceived(
      const proto::FetchCameraRollItemDataResponse& response);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_
