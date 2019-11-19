// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ping_message_handler.h"

#include "components/sync/protocol/sharing_message.pb.h"

PingMessageHandler::PingMessageHandler() = default;

PingMessageHandler::~PingMessageHandler() = default;

void PingMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  // Delibrately empty.
  std::move(done_callback).Run(/*response=*/nullptr);
}
