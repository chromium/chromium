
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include <memory>

#include "chrome/browser/sharing/sharing_device_registration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration()
      : SharingDeviceRegistration(nullptr, nullptr, nullptr, nullptr) {}
  ~FakeSharingDeviceRegistration() override = default;

  bool IsSharedClipboardSupported() const override {
    return shared_clipboard_supported_;
  }

  void SetIsSharedClipboardSupported(bool supported) {
    shared_clipboard_supported_ = supported;
  }

 private:
  bool shared_clipboard_supported_ = false;
};

class SharingHandlerRegistryImplTest : public testing::Test {
 public:
  SharingHandlerRegistryImplTest() = default;
  ~SharingHandlerRegistryImplTest() override = default;

  std::unique_ptr<SharingHandlerRegistryImpl> CreateHandlerRegistry() {
    return std::make_unique<SharingHandlerRegistryImpl>(
        /*profile=*/nullptr, &sharing_device_registration_,
        /*message_sender=*/nullptr, /*device_source=*/nullptr,
        /*sms_fetcher=*/nullptr);
  }

 protected:
  FakeSharingDeviceRegistration sharing_device_registration_;
};

}  // namespace

TEST_F(SharingHandlerRegistryImplTest, SharedClipboard_IsAdded) {
  sharing_device_registration_.SetIsSharedClipboardSupported(true);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kSharedClipboardMessage));
}

TEST_F(SharingHandlerRegistryImplTest, SharedClipboard_NotAdded) {
  sharing_device_registration_.SetIsSharedClipboardSupported(false);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kSharedClipboardMessage));
}
