// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_HANDLER_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingMessageHandler : public SharingMessageHandler {
 public:
  MockSharingMessageHandler();
  MockSharingMessageHandler(const MockSharingMessageHandler&) = delete;
  MockSharingMessageHandler& operator=(const MockSharingMessageHandler&) =
      delete;
  ~MockSharingMessageHandler() override;

  // SharingMessageHandler:
  MOCK_METHOD2(OnMessage,
               void(chrome_browser_sharing::SharingMessage, DoneCallback));
};

#endif  // CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_HANDLER_H_
