// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <map>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/proto/remote_copy_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/shared_clipboard/remote_copy_handle_message_result.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kText[] = "clipboard text";
const char kEmptyDeviceName[] = "";
const char kDeviceNameInMessage[] = "DeviceNameInMessage";
const char16_t kDeviceNameInMessage16[] = u"DeviceNameInMessage";
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

}  // namespace

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

 protected:
  components_sharing_message::SharingMessage CreateMessageWithText(
      const std::string& guid,
      const std::string& device_name,
      const std::string& text) {
    components_sharing_message::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(guid, device_name);
    message.mutable_remote_copy_message()->set_text(text);
    return message;
  }

  components_sharing_message::SharingMessage CreateMessageWithImage(
      const std::string& image_url) {
    image_url_ = image_url;
    image_ = gfx::test::CreateBitmap(10, 20, SK_ColorRED);

    components_sharing_message::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(
            base::Uuid::GenerateRandomV4().AsLowercaseString(),
            kDeviceNameInMessage);
    message.mutable_remote_copy_message()->set_image_url(image_url);
    return message;
  }

  void SetAllowedOrigin(const std::string& origin) {
    message_handler_->set_allowed_origin_for_testing(GURL(origin));
  }

  // Intercepts network requests.
  bool HandleRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (!image_ || params->url_request.url != GURL(image_url_))
      return false;

    content::URLLoaderInterceptor::WriteResponse(
        std::string(), SkBitmapToPNGString(*image_), params->client.get());
    return true;
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
  std::optional<SkBitmap> image_;
};

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithoutDeviceName) {
  message_handler_->OnMessage(
      CreateMessageWithText(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            kEmptyDeviceName, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT_UNKNOWN_DEVICE),
      GetNotification().title());
}

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithDeviceName) {
  message_handler_->OnMessage(
      CreateMessageWithText(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            kDeviceNameInMessage, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                kDeviceNameInMessage16),
            GetNotification().title());
}

TEST_F(RemoteCopyMessageHandlerTest, IsImageSourceAllowed) {
  std::string image_url = "https://foo.com/image.png";
  std::string image_url_with_subdomain = "https://www.foo.com/image.png";

  SetAllowedOrigin("https://foo.com");
  EXPECT_TRUE(message_handler_->IsImageSourceAllowed(GURL(image_url)));
  EXPECT_TRUE(
      message_handler_->IsImageSourceAllowed(GURL(image_url_with_subdomain)));

  SetAllowedOrigin("https://bar.com");
  EXPECT_FALSE(message_handler_->IsImageSourceAllowed(GURL(image_url)));

  SetAllowedOrigin("https://foo.com:80");  // not default port
  EXPECT_FALSE(message_handler_->IsImageSourceAllowed(GURL(image_url)));

  SetAllowedOrigin("https://foo.com:443");  // default port
  EXPECT_TRUE(message_handler_->IsImageSourceAllowed(GURL(image_url)));
}

TEST_F(RemoteCopyMessageHandlerTest, HandleImage) {
  SetAllowedOrigin(kTestImageUrl);
  base::RunLoop run_loop;
  ClipboardObserver observer(run_loop.QuitClosure());
  ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);

  message_handler_->OnMessage(CreateMessageWithImage(kTestImageUrl),
                              base::DoNothing());

  // Let tasks run until the image is decoded, written to the clipboard and the
  // simple notification is shown.
  run_loop.Run();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);

  // Expect the image to be in the clipboard now.
  SkBitmap image = GetClipboardImage();
  EXPECT_TRUE(gfx::BitmapsAreEqual(*image_, image));

  // Expect a simple notification.
  auto notification = GetNotification();
  EXPECT_TRUE(notification.image().IsEmpty());
}
