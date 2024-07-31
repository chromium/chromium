// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include <memory>

#include "components/sharing_message/mock_sharing_message_handler.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration()
      : SharingDeviceRegistration(/*pref_service=*/nullptr,
                                  /*sharing_sync_preference=*/nullptr,
                                  /*vapid_key_manager=*/nullptr,
                                  /*instance_id_driver=*/nullptr,
                                  /*sync_service=*/nullptr) {}
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
  content::BrowserTaskEnvironment task_environment_;
  FakeSharingDeviceRegistration sharing_device_registration_;
};

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SharingHandlerRegistryImplTest, SharedClipboard_IsAdded) {
  sharing_device_registration_.SetIsSharedClipboardSupported(true);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));

  // Default handlers cannot be removed.
  handler_registry->UnregisterSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage);
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));
}

TEST_F(SharingHandlerRegistryImplTest, SharedClipboard_NotAdded) {
  sharing_device_registration_.SetIsSharedClipboardSupported(false);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(SharingHandlerRegistryImplTest, SharedClipboard_AddRemoveManually) {
  sharing_device_registration_.SetIsSharedClipboardSupported(false);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));

  handler_registry->RegisterSharingHandler(
      std::make_unique<MockSharingMessageHandler>(),
      components_sharing_message::SharingMessage::kSharedClipboardMessage);
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));

  handler_registry->UnregisterSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage);
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSharedClipboardMessage));
}
