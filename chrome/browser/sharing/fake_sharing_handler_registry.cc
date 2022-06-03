// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/fake_sharing_handler_registry.h"

#include "base/notreached.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

FakeSharingHandlerRegistry::FakeSharingHandlerRegistry() = default;
FakeSharingHandlerRegistry::~FakeSharingHandlerRegistry() = default;

SharingMessageHandler* FakeSharingHandlerRegistry::GetSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  auto it = handler_map_.find(payload_case);
  return it != handler_map_.end() ? it->second : nullptr;
}

void FakeSharingHandlerRegistry::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  NOTIMPLEMENTED();
}

void FakeSharingHandlerRegistry::UnregisterSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  NOTIMPLEMENTED();
}

void FakeSharingHandlerRegistry::SetSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case,
    SharingMessageHandler* handler) {
  handler_map_[payload_case] = handler;
}
