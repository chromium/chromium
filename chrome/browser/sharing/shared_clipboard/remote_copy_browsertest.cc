// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
const char kDeviceName[] = "test device name";
const char kText[] = "test text";
const char kHistogramName[] = "Sharing.RemoteCopyHandleMessageResult";

class ClipboardObserver : public ui::ClipboardObserver {
 public:
  explicit ClipboardObserver(base::RepeatingClosure callback)
      : callback_(callback) {}

  void OnClipboardDataChanged() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardObserver);
};
}  // namespace

// Browser tests for the Remote Copy feature.
class RemoteCopyBrowserTestBase : public InProcessBrowserTest {
 public:
  RemoteCopyBrowserTestBase() = default;
  ~RemoteCopyBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    ui::TestClipboard::CreateForCurrentThread();
    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    sharing_service_ =
        SharingServiceFactory::GetForBrowserContext(browser()->profile());
  }

  void TearDownOnMainThread() override {
    notification_tester_.reset();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  gcm::IncomingMessage CreateMessage(const std::string& device_name,
                                     base::Optional<std::string> text,
                                     base::Optional<GURL> image_url) {
    chrome_browser_sharing::SharingMessage sharing_message;
    sharing_message.set_sender_guid(base::GenerateGUID());
    sharing_message.set_sender_device_name(device_name);
    if (text) {
      sharing_message.mutable_remote_copy_message()->set_text(text.value());
    } else if (image_url) {
      sharing_message.mutable_remote_copy_message()->set_image_url(
          image_url.value().possibly_invalid_spec());
    }

    gcm::IncomingMessage incoming_message;
    std::string serialized_sharing_message;
    sharing_message.SerializeToString(&serialized_sharing_message);
    incoming_message.raw_data = serialized_sharing_message;
    return incoming_message;
  }

  void SendTextMessage(const std::string& device_name,
                       const std::string& text) {
    sharing_service_->GetFCMHandlerForTesting()->OnMessage(
        kSharingFCMAppID,
        CreateMessage(device_name, text, /*image_url=*/base::nullopt));
  }

  void SendImageMessage(const std::string& device_name, const GURL& image_url) {
    base::RunLoop run_loop;
    ClipboardObserver observer(run_loop.QuitClosure());
    ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);
    sharing_service_->GetFCMHandlerForTesting()->OnMessage(
        kSharingFCMAppID,
        CreateMessage(device_name, /*text*/ base::nullopt, image_url));
    run_loop.Run();
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);
  }

  std::vector<base::string16> GetAvailableClipboardTypes() {
    std::vector<base::string16> types;
    bool contains_filenames;
    ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
        ui::ClipboardBuffer::kCopyPaste, &types, &contains_filenames);
    return types;
  }

  std::string ReadClipboardText() {
    base::string16 text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, &text);
    return base::UTF16ToUTF8(text);
  }

  SkBitmap ReadClipboardImage() {
    return ui::Clipboard::GetForCurrentThread()->ReadImage(
        ui::ClipboardBuffer::kCopyPaste);
  }

  message_center::Notification GetNotification() {
    auto notifications = notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::SHARING);
    EXPECT_EQ(notifications.size(), 1u);

    const message_center::Notification& notification = notifications[0];
    EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

    return notification;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  SharingService* sharing_service_;
  std::unique_ptr<net::EmbeddedTestServer> server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCopyBrowserTestBase);
};

class RemoteCopyDisabledBrowserTest : public RemoteCopyBrowserTestBase {
 public:
  RemoteCopyDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kRemoteCopyReceiver);
  }
};

IN_PROC_BROWSER_TEST_F(RemoteCopyDisabledBrowserTest, FeatureDisabled) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The clipboard is still empty because the feature is disabled and the
  // handler is not installed.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());
  histograms_.ExpectTotalCount(kHistogramName, 0);
}

class RemoteCopyBrowserTest : public RemoteCopyBrowserTestBase {
 public:
  RemoteCopyBrowserTest() {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    EXPECT_TRUE(server_->Start());

    url::Origin allowlist_origin = url::Origin::Create(server_->base_url());
    feature_list_.InitAndEnableFeatureWithParameters(
        kRemoteCopyReceiver,
        {{kRemoteCopyAllowedOrigins.name, allowlist_origin.Serialize()}});
  }
};

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, Text) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard and a notification is shown.
  std::vector<base::string16> types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  ASSERT_EQ(kText, ReadClipboardText());
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceName)),
            GetNotification().title());
  histograms_.ExpectUniqueSample(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);
}

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, ImageUrl) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with an image url.
  SendImageMessage(kDeviceName, server_->GetURL("/image_decoding/droids.jpg"));

  // The image is in the clipboard and a notification is shown.
  std::vector<base::string16> types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypePNG, base::UTF16ToASCII(types[0]));
  SkBitmap bitmap = ReadClipboardImage();
  ASSERT_FALSE(bitmap.drawsNothing());
  ASSERT_EQ(2560, bitmap.width());
  ASSERT_EQ(1920, bitmap.height());
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceName)),
            GetNotification().title());
  histograms_.ExpectUniqueSample(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledImage, 1);
}

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, TextThenImageUrl) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());
  histograms_.ExpectTotalCount(kHistogramName, 0);

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard.
  std::vector<base::string16> types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  ASSERT_EQ(kText, ReadClipboardText());
  histograms_.ExpectTotalCount(kHistogramName, 1);
  histograms_.ExpectUniqueSample(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);

  // Send a message with an image url.
  SendImageMessage(kDeviceName, server_->GetURL("/image_decoding/droids.jpg"));

  // The image is in the clipboard and the text has been cleared.
  types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypePNG, base::UTF16ToASCII(types[0]));
  ASSERT_EQ(std::string(), ReadClipboardText());
  histograms_.ExpectTotalCount(kHistogramName, 2);
  histograms_.ExpectBucketCount(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledImage, 1);
}
