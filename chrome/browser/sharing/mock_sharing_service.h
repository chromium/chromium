// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_MOCK_SHARING_SERVICE_H_
#define CHROME_BROWSER_SHARING_MOCK_SHARING_SERVICE_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingService : public SharingService {
 public:
  MockSharingService();

  MockSharingService(const MockSharingService&) = delete;
  MockSharingService& operator=(const MockSharingService&) = delete;

  ~MockSharingService() override;

  MOCK_CONST_METHOD1(
      GetDeviceCandidates,
      std::vector<std::unique_ptr<syncer::DeviceInfo>>(
          sync_pb::SharingSpecificFields::EnabledFeatures required_feature));

  MOCK_METHOD4(
      SendMessageToDevice,
      base::OnceClosure(const syncer::DeviceInfo& device,
                        base::TimeDelta response_timeout,
                        chrome_browser_sharing::SharingMessage message,
                        SharingMessageSender::ResponseCallback callback));

  MOCK_CONST_METHOD1(
      GetDeviceByGuid,
      std::unique_ptr<syncer::DeviceInfo>(const std::string& guid));

  MOCK_METHOD2(
      RegisterSharingHandler,
      void(std::unique_ptr<SharingMessageHandler> handler,
           chrome_browser_sharing::SharingMessage::PayloadCase payload_case));

  MOCK_METHOD1(
      UnregisterSharingHandler,
      void(chrome_browser_sharing::SharingMessage::PayloadCase payload_case));
};

#endif  // CHROME_BROWSER_SHARING_MOCK_SHARING_SERVICE_H_
