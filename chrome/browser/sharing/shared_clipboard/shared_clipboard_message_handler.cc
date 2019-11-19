// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/sharing_device_source.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync/protocol/sharing_shared_clipboard_message.pb.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandler::SharedClipboardMessageHandler(
    SharingDeviceSource* device_source)
    : device_source_(device_source) {}

SharedClipboardMessageHandler::~SharedClipboardMessageHandler() = default;

void SharedClipboardMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_shared_clipboard_message());

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.shared_clipboard_message().text()));

  std::unique_ptr<syncer::DeviceInfo> device =
      device_source_->GetDeviceByGuid(message.sender_guid());
  const std::string& device_name =
      device ? device->client_name() : message.sender_device_name();
  ShowNotification(device_name);

  std::move(done_callback).Run(/*response=*/nullptr);
}
