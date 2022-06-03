// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/mock_sharing_message_sender.h"

MockSharingMessageSender::MockSharingMessageSender()
    : SharingMessageSender(
          /*local_device_info_provider=*/nullptr) {}

MockSharingMessageSender::~MockSharingMessageSender() = default;
