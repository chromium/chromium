// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/sharing_message/proto/shared_clipboard_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_device_source.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandler::SharedClipboardMessageHandler(
    SharingDeviceSource* device_source)
    : device_source_(device_source) {}

SharedClipboardMessageHandler::~SharedClipboardMessageHandler() = default;

void SharedClipboardMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_shared_clipboard_message());
  TRACE_EVENT0("sharing", "SharedClipboardMessageHandler::OnMessage");

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.shared_clipboard_message().text()));

  std::optional<SharingTargetDeviceInfo> device =
      device_source_->GetDeviceByGuid(message.sender_guid());
  const std::string& device_name =
      device.has_value() ? device->client_name() : message.sender_device_name();
  ShowNotification(device_name);

  std::move(done_callback).Run(/*response=*/nullptr);
}
