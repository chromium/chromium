// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_

#include <memory>

#include "base/callback.h"

namespace chrome_browser_sharing {
class SharingMessage;
class ResponseMessage;
}  // namespace chrome_browser_sharing

// Interface for handling incoming SharingMessage.
class SharingMessageHandler {
 public:
  using DoneCallback = base::OnceCallback<void(
      std::unique_ptr<chrome_browser_sharing::ResponseMessage>)>;

  virtual ~SharingMessageHandler() = default;

  // Called when a SharingMessage has been received. |done_callback| must be
  // invoked after work to determine response is done.
  virtual void OnMessage(chrome_browser_sharing::SharingMessage message,
                         DoneCallback done_callback) = 0;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_
