// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <map>
#include <string>

#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/proto/remote_copy_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"
#include "ui/message_center/public/cpp/notification.h"

#if defined(OS_WIN)
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#endif  // defined(OS_WIN)

namespace {

const char kText[] = "clipboard text";
const char kEmptyDeviceName[] = "";
const char kDeviceNameInMessage[] = "DeviceNameInMessage";
const char kHistogramName[] = "Sharing.RemoteCopyHandleMessageResult";
const char kTestImageUrl[] = "https://foo.com/image.png";

class ClipboardObserver : public ui::ClipboardObserver {
 public:
  explicit ClipboardObserver(base::RepeatingClosure callback)
      : callback_(callback) {}
  ClipboardObserver(const ClipboardObserver&) = delete;
  ClipboardObserver& operator=(const ClipboardObserver&) = delete;
  ~ClipboardObserver() override = default;

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

class RemoteCopyMessageHandlerTest : public SharedClipboardTestBase {
 public:
  RemoteCopyMessageHandlerTest()
      : url_loader_interceptor_(
            base::BindRepeating(&RemoteCopyMessageHandlerTest::HandleRequest,
                                base::Unretained(this))) {}

  ~RemoteCopyMessageHandlerTest() override = default;

  void SetUp() override {
    SharedClipboardTestBase::SetUp();
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<MockSharingService>>();
        }));
    message_handler_ = std::make_unique<RemoteCopyMessageHandler>(&profile_);
  }

  chrome_browser_sharing::SharingMessage CreateMessageWithText(
      const std::string& guid,
      const std::string& device_name,
      const std::string& text) {
    chrome_browser_sharing::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(guid, device_name);
    message.mutable_remote_copy_message()->set_text(text);
    return message;
  }

  chrome_browser_sharing::SharingMessage CreateMessageWithImage(
      const std::string& image_url) {
    image_url_ = image_url;
    image_ = CreateTestSkBitmap(/*w=*/10, /*h=*/20, SK_ColorRED);

    chrome_browser_sharing::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(base::GenerateGUID(),
                                               kDeviceNameInMessage);
    message.mutable_remote_copy_message()->set_image_url(image_url);
    return message;
  }

  bool IsImageSourceAllowed(const std::string& image_url,
                            const std::string& param_value) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, param_value}});
    return message_handler_->IsImageSourceAllowed(GURL(image_url));
  }

 protected:
  // Intercepts network requests.
  bool HandleRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (!image_ || params->url_request.url != GURL(image_url_))
      return false;

    content::URLLoaderInterceptor::WriteResponse(
        std::string(), SkBitmapToPNGString(*image_), params->client.get());
    return true;
  }

  static SkBitmap CreateTestSkBitmap(int w, int h, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(w, h);
    bitmap.eraseColor(color);
    return bitmap;
  }

  static std::string SkBitmapToPNGString(const SkBitmap& bitmap) {
    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                      &png_data);
    return std::string(png_data.begin(), png_data.end());
  }

  std::unique_ptr<RemoteCopyMessageHandler> message_handler_;
  base::HistogramTester histograms_;
  content::URLLoaderInterceptor url_loader_interceptor_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::string image_url_;
  base::Optional<SkBitmap> image_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCopyMessageHandlerTest);
};

}  // namespace

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithoutDeviceName) {
  message_handler_->OnMessage(
      CreateMessageWithText(base::GenerateGUID(), kEmptyDeviceName, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT_UNKNOWN_DEVICE),
      GetNotification().title());
  histograms_.ExpectUniqueSample(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);
}

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithDeviceName) {
  message_handler_->OnMessage(
      CreateMessageWithText(base::GenerateGUID(), kDeviceNameInMessage, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                base::ASCIIToUTF16(kDeviceNameInMessage)),
            GetNotification().title());
  histograms_.ExpectUniqueSample(
      kHistogramName, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);
}

TEST_F(RemoteCopyMessageHandlerTest, IsImageSourceAllowed) {
  std::string image_url = "https://foo.com/image.png";
  std::string image_url_with_subdomain = "https://www.foo.com/image.png";
  EXPECT_TRUE(IsImageSourceAllowed(image_url, "https://foo.com"));
  EXPECT_TRUE(
      IsImageSourceAllowed(image_url_with_subdomain, "https://foo.com"));
  EXPECT_FALSE(IsImageSourceAllowed(image_url, ""));
  EXPECT_FALSE(IsImageSourceAllowed(image_url, "foo][#';/.,"));
  EXPECT_FALSE(IsImageSourceAllowed(image_url, "https://bar.com"));
  EXPECT_FALSE(IsImageSourceAllowed(image_url,
                                    "https://foo.com:80"));  // not default port
  EXPECT_TRUE(
      IsImageSourceAllowed(image_url, "https://foo.com:443"));  // default port
  EXPECT_TRUE(
      IsImageSourceAllowed(image_url, "https://foo.com,https://bar.com"));
  EXPECT_TRUE(
      IsImageSourceAllowed(image_url, "https://bar.com,https://foo.com"));
}

TEST_F(RemoteCopyMessageHandlerTest,
       NoProgressNotificationWithoutProgressFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver,
        {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}}},
      {kRemoteCopyProgressNotification});

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  EXPECT_FALSE(HasProgressNotification());

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}

TEST_F(RemoteCopyMessageHandlerTest, ProgressNotificationWithProgressFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}},
       {kRemoteCopyProgressNotification, {}}},
      {});

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  ASSERT_TRUE(HasProgressNotification());
  auto notification = GetProgressNotification();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT,
                base::ASCIIToUTF16(kDeviceNameInMessage)),
            notification.title());

#if defined(OS_MAC)
  // On macOS the progress status is shown in the message.
  std::u16string progress_status = notification.message();
#else
  std::u16string progress_status = notification.progress_status();
#endif  // defined(OS_MAC)

#if defined(OS_WIN)
  std::u16string expected_status = l10n_util::GetStringUTF16(
      NotificationPlatformBridgeWin::SystemNotificationEnabled()
          ? IDS_SHARING_REMOTE_COPY_NOTIFICATION_PROCESSING_IMAGE
          : IDS_SHARING_REMOTE_COPY_NOTIFICATION_PREPARING_DOWNLOAD);
#else
  std::u16string expected_status = l10n_util::GetStringUTF16(
      IDS_SHARING_REMOTE_COPY_NOTIFICATION_PREPARING_DOWNLOAD);
#endif  // defined(OS_WIN)

  EXPECT_EQ(expected_status, progress_status);
  EXPECT_EQ(-1, notification.progress());

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}

TEST_F(RemoteCopyMessageHandlerTest, ImageNotificationWithoutProgressFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}},
       {kRemoteCopyImageNotification, {}}},
      {kRemoteCopyProgressNotification});

  base::RunLoop run_loop;
  ClipboardObserver observer(run_loop.QuitClosure());
  ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  // There should not be a progress notification without the flag set.
  EXPECT_FALSE(HasProgressNotification());

  // Let tasks run until the image is decoded, written to the clipboard and the
  // image notification is shown.
  run_loop.Run();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);

  // Expect the image to be in the clipboard now.
  SkBitmap image = GetClipboardImage();
  EXPECT_TRUE(gfx::BitmapsAreEqual(*image_, image));

  // Expect an image notification showing the image.
  auto notification = GetImageNotification();

#if defined(OS_MAC)
  // On macOS we show the image as the icon instead.
  EXPECT_FALSE(notification.icon().IsEmpty());
#else
  EXPECT_FALSE(notification.image().IsEmpty());
#endif  // defined(OS_MAC)

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}

TEST_F(RemoteCopyMessageHandlerTest,
       NoImageAndNoProgressNotificationWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver,
        {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}}},
      {kRemoteCopyImageNotification, kRemoteCopyProgressNotification});

  base::RunLoop run_loop;
  ClipboardObserver observer(run_loop.QuitClosure());
  ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  // There should be no progress notification with the flag disabled.
  EXPECT_FALSE(HasProgressNotification());

  // Let tasks run until the image is decoded, written to the clipboard and the
  // simple notification is shown (the image notification feature is disabled).
  run_loop.Run();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);

  // After finishing the transfer there should be no progress notification.
  EXPECT_FALSE(HasProgressNotification());

  // Expect the image to be in the clipboard now.
  SkBitmap image = GetClipboardImage();
  EXPECT_TRUE(gfx::BitmapsAreEqual(*image_, image));

  // Expect a simple notification.
  auto notification = GetNotification();
  EXPECT_TRUE(notification.image().IsEmpty());

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}

TEST_F(RemoteCopyMessageHandlerTest, ImageNotificationWithProgressFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}},
       {kRemoteCopyImageNotification, {}},
       {kRemoteCopyProgressNotification, {}}},
      {});

  base::RunLoop run_loop;
  ClipboardObserver observer(run_loop.QuitClosure());
  ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  // There should be a progress notification with the flag set.
  EXPECT_TRUE(HasProgressNotification());

  // Let tasks run until the image is decoded, written to the clipboard and the
  // image notification is shown.
  run_loop.Run();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);

  // After finishing the transfer there should be no progress notification.
  EXPECT_FALSE(HasProgressNotification());

  // Expect the image to be in the clipboard now.
  SkBitmap image = GetClipboardImage();
  EXPECT_TRUE(gfx::BitmapsAreEqual(*image_, image));

  // Expect an image notification showing the image.
  auto notification = GetImageNotification();

#if defined(OS_MAC)
  // On macOS we show the image as the icon instead.
  EXPECT_FALSE(notification.icon().IsEmpty());
#else
  EXPECT_FALSE(notification.image().IsEmpty());
#endif  // defined(OS_MAC)

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}

TEST_F(RemoteCopyMessageHandlerTest, CancelProgressNotification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}},
       {kRemoteCopyProgressNotification, {}}},
      {});

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());
  auto notification = GetProgressNotification();

  // Simulate a click on the cancel button at index 0.
  notification_tester_->SimulateClick(NotificationHandler::Type::SHARING,
                                      notification.id(), /*action_index=*/0,
                                      /*reply=*/base::nullopt);

  // The progress notification should now be closed.
  EXPECT_FALSE(HasProgressNotification());

  // Run remaining tasks to ensure no notification is shown at the end.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(HasProgressNotification());
  EXPECT_FALSE(HasImageNotification());
}

TEST_F(RemoteCopyMessageHandlerTest, DismissProgressNotification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kRemoteCopyReceiver, {{kRemoteCopyAllowedOrigins.name, kTestImageUrl}}},
       {kRemoteCopyImageNotification, {}},
       {kRemoteCopyProgressNotification, {}}},
      {});

  base::RunLoop run_loop;
  ClipboardObserver observer(run_loop.QuitClosure());
  ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());
  auto notification = GetProgressNotification();

  // Simulate closing the notification by the user.
  notification_tester_->RemoveNotification(NotificationHandler::Type::SHARING,
                                           notification.id(), /*by_user=*/true,
                                           /*silent=*/false);

  // The progress notification should now be closed.
  EXPECT_FALSE(HasProgressNotification());

  // Let tasks run until the image is decoded, written to the clipboard and the
  // image notification is shown.
  run_loop.Run();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);

  EXPECT_TRUE(HasImageNotification());

  // Calling GetDefaultStoragePartition creates tasks that need to run before
  // the ScopedFeatureList is destroyed. See crbug.com/1060869
  task_environment_.RunUntilIdle();
}
