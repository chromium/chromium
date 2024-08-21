// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/browser/ui/ash/session/test_session_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/icons/vector_icons.h"
#endif

using ::testing::_;
using testing::Return;

namespace {

const uint64_t kWifiCredentialsId = 111;
const char kTextBody[] = "text body";
const char kTextUrl[] = "https://google.com";
const char kWifiSsid[] = "test_ssid";
const WifiCredentialsAttachment::SecurityType kSecurityType =
    sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

MATCHER_P(MatchesTarget, target, "") {
  return arg.id == target.id;
}

class MockSettingsOpener : public NearbyNotificationManager::SettingsOpener {
 public:
  MOCK_METHOD(void,
              ShowSettingsPage,
              (Profile*, const std::string&),
              (override));
};

TextAttachment CreateTextAttachment(TextAttachment::Type type) {
  return TextAttachment(type, kTextBody, /*title=*/std::nullopt,
                        /*mime_type=*/std::nullopt);
}

TextAttachment CreateUrlAttachment() {
  return TextAttachment(TextAttachment::Type::kUrl, kTextUrl,
                        /*title=*/std::nullopt, /*mime_type=*/std::nullopt);
}

FileAttachment CreateFileAttachment(FileAttachment::Type type) {
  return FileAttachment(/*id=*/0, /*size=*/10, /*file_name=*/"file.jpg",
                        /*mime_type=*/"example", type);
}

WifiCredentialsAttachment CreateWifiCredentialsAttachment(
    WifiCredentialsAttachment::SecurityType securityType) {
  return WifiCredentialsAttachment(kWifiCredentialsId, securityType, kWifiSsid);
}

std::unique_ptr<KeyedService> CreateMockNearbySharingService(
    content::BrowserContext* browser_context) {
  return std::make_unique<testing::NiceMock<MockNearbySharingService>>();
}

MockNearbySharingService* CreateAndUseMockNearbySharingService(
    content::BrowserContext* browser_context) {
  return static_cast<MockNearbySharingService*>(
      NearbySharingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context,
          base::BindRepeating(&CreateMockNearbySharingService)));
}

std::string GetClipboardText() {
  std::u16string text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
  return base::UTF16ToUTF8(text);
}

SkBitmap GetClipboardImage() {
  SkBitmap bitmap;
  std::vector<uint8_t> png_data =
      ui::clipboard_test_util::ReadPng(ui::Clipboard::GetForCurrentThread());
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  return bitmap;
}

SkBitmap CreateTestSkBitmap() {
  return gfx::test::CreateBitmap(10, 15, SK_ColorRED);
}

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

class NearbyNotificationManagerTest : public testing::Test {
 public:
  NearbyNotificationManagerTest() {
    RegisterNearbySharingPrefs(pref_service_.registry());
  }
  ~NearbyNotificationManagerTest() override = default;

  void SetUp() override {
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(true);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);
    nearby_service_ = CreateAndUseMockNearbySharingService(&profile_);
    manager_ = CreateManager();

    std::unique_ptr<MockSettingsOpener> settings_opener =
        std::make_unique<MockSettingsOpener>();
    settings_opener_ = settings_opener.get();
    manager_->SetSettingsOpenerForTesting(std::move(settings_opener));

    EXPECT_CALL(*nearby_service_, GetNotificationDelegate(testing::_))
        .WillRepeatedly(
            testing::Invoke([&](const std::string& notification_id) {
              return manager_->GetNotificationDelegate(notification_id);
            }));

    DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(&profile_));

    ui::TestClipboard::CreateForCurrentThread();

    // From now on we don't allow any blocking tasks anymore.
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
        ->SetDownloadManagerDelegateForTesting(nullptr);
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  NearbyNotificationManager* manager() { return manager_.get(); }

  std::vector<message_center::Notification> GetDisplayedNotifications() {
    return notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::NEARBY_SHARE);
  }

  std::unique_ptr<NearbyNotificationManager> CreateManager() {
    NotificationDisplayService* notification_display_service =
        NotificationDisplayServiceFactory::GetForProfile(&profile_);
    return std::make_unique<NearbyNotificationManager>(
        notification_display_service, nearby_service_, &pref_service_,
        &profile_);
  }

  ShareTarget CreateIncomingShareTarget(int text_attachments,
                                        int url_attachements,
                                        int image_attachments,
                                        int other_file_attachments,
                                        bool wifi_credentials_attachments) {
    ShareTarget share_target;
    share_target.is_incoming = true;
    for (int i = 0; i < text_attachments; i++) {
      share_target.text_attachments.push_back(
          CreateTextAttachment(TextAttachment::Type::kText));
    }

    for (int i = 0; i < url_attachements; i++) {
      share_target.text_attachments.push_back(CreateUrlAttachment());
    }

    for (int i = 0; i < image_attachments; i++) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      std::string name = base::StrCat({base::NumberToString(i), ".png"});
      base::FilePath file_path = temp_dir_.GetPath().AppendASCII(name);
      SkBitmap image = CreateTestSkBitmap();

      std::vector<unsigned char> png_data;
      EXPECT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(
          image, /*discard_transparency=*/true, &png_data));
      base::WriteFile(file_path, png_data);

      FileAttachment attachment(file_path);
      share_target.file_attachments.push_back(std::move(attachment));
    }

    for (int i = 0; i < other_file_attachments; i++) {
      share_target.file_attachments.push_back(
          CreateFileAttachment(FileAttachment::Type::kVideo));
    }

    if (wifi_credentials_attachments) {
      share_target.wifi_credentials_attachments.push_back(
          CreateWifiCredentialsAttachment(
              WifiCredentialsAttachment::SecurityType::kWpaPsk));
    }
    return share_target;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingPrefServiceSimple pref_service_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  raw_ptr<MockNearbySharingService> nearby_service_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<NearbyNotificationManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  raw_ptr<MockSettingsOpener, DanglingUntriaged> settings_opener_;
};

struct AttachmentsTestParamInternal {
  std::vector<TextAttachment::Type> text_attachments;
  std::vector<FileAttachment::Type> file_attachments;
  std::vector<WifiCredentialsAttachment::SecurityType>
      wifi_credentials_attachments;
  int expected_capitalized_resource_id;
  int expected_not_capitalized_resource_id;
};

AttachmentsTestParamInternal kAttachmentsTestParams[] = {
    // No attachments.
    {{},
     {},
     {},
     IDS_NEARBY_CAPITALIZED_UNKNOWN_ATTACHMENTS,
     IDS_NEARBY_NOT_CAPITALIZED_UNKNOWN_ATTACHMENTS},

    // Mixed attachments.
    {
        {TextAttachment::Type::kText},
        {FileAttachment::Type::kUnknown},
        {},
        IDS_NEARBY_CAPITALIZED_UNKNOWN_ATTACHMENTS,
        IDS_NEARBY_NOT_CAPITALIZED_UNKNOWN_ATTACHMENTS,
    },

    // Text attachments.
    {{TextAttachment::Type::kUrl},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_LINKS,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_LINKS},
    {{TextAttachment::Type::kText},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_UNKNOWN,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN},
    {{TextAttachment::Type::kAddress},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_ADDRESSES,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_ADDRESSES},
    {{TextAttachment::Type::kPhoneNumber},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_PHONE_NUMBERS,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_PHONE_NUMBERS},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kAddress},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_ADDRESSES,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_ADDRESSES},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kUrl},
     {},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_UNKNOWN,
     IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN},

    // File attachments.
    {{},
     {FileAttachment::Type::kApp},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_APPS,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_APPS},
    {{},
     {FileAttachment::Type::kImage},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_IMAGES,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_IMAGES},
    {{},
     {FileAttachment::Type::kUnknown},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_UNKNOWN,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN},
    {{},
     {FileAttachment::Type::kVideo},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_VIDEOS,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_VIDEOS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kApp},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_APPS,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_APPS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kImage},
     {},
     IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_UNKNOWN,
     IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN},

    // Wi-Fi Credentials attachments.
    {{},
     {},
     {WifiCredentialsAttachment::SecurityType::kWpaPsk},
     IDS_NEARBY_CAPITALIZED_UNKNOWN_ATTACHMENTS,
     IDS_NEARBY_NOT_CAPITALIZED_UNKNOWN_ATTACHMENTS},
};

// Boolean parameter is |is_incoming| and the tuple parameter is a feature list
// containing |is_self_share_enabled| and |is_self_share_auto_accept_enabled|.
using AttachmentsTestParam = std::tuple<AttachmentsTestParamInternal, bool>;

class NearbyNotificationManagerAttachmentsTest
    : public NearbyNotificationManagerTest,
      public testing::WithParamInterface<AttachmentsTestParam> {
 public:
  NearbyNotificationManagerAttachmentsTest() = default;
};

// Boolean parameter is |with_token|.
class NearbyNotificationManagerConnectionRequestTest
    : public NearbyNotificationManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  NearbyNotificationManagerConnectionRequestTest() = default;
};

std::u16string FormatNotificationTitle(
    int resource_id,
    const AttachmentsTestParamInternal& param,
    const std::string& device_name,
    const std::string& network_name,
    bool use_capitalized_resource) {
  if (!param.wifi_credentials_attachments.empty()) {
    return base::ReplaceStringPlaceholders(
        l10n_util::GetStringUTF16(resource_id),
        {base::ASCIIToUTF16(network_name), base::ASCIIToUTF16(device_name)},
        /*offsets=*/nullptr);
  }

  size_t total = param.text_attachments.size() + param.file_attachments.size();
  int attachments_resource_id =
      use_capitalized_resource ? param.expected_capitalized_resource_id
                               : param.expected_not_capitalized_resource_id;
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(resource_id, total),
      {l10n_util::GetPluralStringFUTF16(attachments_resource_id, total),
       base::ASCIIToUTF16(device_name)},
      /*offsets=*/nullptr);
}

}  // namespace

TEST_F(NearbyNotificationManagerTest, RegistersAsBackgroundSurfaces) {
  manager_.reset();
  TransferUpdateCallback* receive_transfer_callback = nullptr;
  TransferUpdateCallback* send_transfer_callback = nullptr;
  ShareTargetDiscoveredCallback* send_discovery_callback = nullptr;

  EXPECT_CALL(
      *nearby_service_,
      RegisterReceiveSurface(
          testing::_, NearbySharingService::ReceiveSurfaceState::kBackground))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&receive_transfer_callback),
          testing::Return(NearbySharingService::StatusCodes::kOk)));
  EXPECT_CALL(
      *nearby_service_,
      RegisterSendSurface(testing::_, testing::_,
                          NearbySharingService::SendSurfaceState::kBackground))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&send_transfer_callback),
          testing::SaveArg<1>(&send_discovery_callback),
          testing::Return(NearbySharingService::StatusCodes::kOk)));
  manager_ = CreateManager();

  EXPECT_EQ(manager(), receive_transfer_callback);
  EXPECT_EQ(manager(), send_transfer_callback);
  EXPECT_EQ(manager(), send_discovery_callback);
}

TEST_F(NearbyNotificationManagerTest, UnregistersSurfaces) {
  EXPECT_CALL(*nearby_service_, UnregisterReceiveSurface(manager()));
  EXPECT_CALL(*nearby_service_, UnregisterSendSurface(manager(), manager()));
  manager_.reset();
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_ShowsNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  ShareTarget share_target;
  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();

  manager()->ShowProgress(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());
  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_TRUE(notification.pinned());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(1u, buttons.size());

  const message_center::ButtonInfo& cancel_button = buttons[0];
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), cancel_button.title);
}

TEST_F(NearbyNotificationManagerTest,
       ShowProgress_ShowsNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  ShareTarget share_target;
  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();

  manager()->ShowProgress(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());
  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_TRUE(notification.pinned());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(1u, buttons.size());

  const message_center::ButtonInfo& cancel_button = buttons[0];
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), cancel_button.title);
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_ShowsProgress) {
  double progress = 75.0;

  ShareTarget share_target;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder().set_progress(progress).build();

  manager()->ShowProgress(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(progress, notification.progress());
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_UpdatesProgress) {
  ShareTarget share_target;
  TransferMetadataBuilder transfer_metadata_builder;
  transfer_metadata_builder.set_progress(75.0);

  manager()->ShowProgress(share_target, transfer_metadata_builder.build());

  double progress = 50.0;
  transfer_metadata_builder.set_progress(progress);
  manager()->ShowProgress(share_target, transfer_metadata_builder.build());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(progress, notification.progress());
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_DeviceNameEncoding) {
  ShareTarget share_target;
  share_target.device_name = "🌵";
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder().set_progress(75.0).build();

  manager()->ShowProgress(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  std::string title = base::UTF16ToUTF8(notifications[0].title());
  EXPECT_TRUE(title.find(share_target.device_name) != std::string::npos);
}

TEST_P(NearbyNotificationManagerAttachmentsTest, ShowProgress) {
  const AttachmentsTestParamInternal& param = std::get<0>(GetParam());
  bool is_incoming = std::get<1>(GetParam());

  if (!param.wifi_credentials_attachments.empty())
    return;

  std::string device_name = "device";
  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.is_incoming = is_incoming;

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  if (is_incoming) {
    for (WifiCredentialsAttachment::SecurityType securityType :
         param.wifi_credentials_attachments) {
      share_target.wifi_credentials_attachments.push_back(
          CreateWifiCredentialsAttachment(securityType));
    }
  }

  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();
  manager()->ShowProgress(share_target, transfer_metadata);

  std::u16string expected;
  if (!param.wifi_credentials_attachments.empty() && is_incoming) {
    expected = FormatNotificationTitle(
        IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE_WIFI_CREDENTIALS, param,
        device_name, share_target.wifi_credentials_attachments[0].ssid(),
        /*use_capitalized_resource=*/false);
  } else {
    expected = FormatNotificationTitle(
        is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
                    : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE,
        param, device_name, /*network_name=*/kWifiSsid,
        /*use_capitalized_resource=*/false);
  }

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(expected, notification.title());
}

TEST_P(NearbyNotificationManagerAttachmentsTest, ShowSuccess) {
  const AttachmentsTestParamInternal& param = std::get<0>(GetParam());
  bool is_incoming = std::get<1>(GetParam());

  std::string device_name = "device";
  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.is_incoming = is_incoming;

  // We currently do not support sending Wi-Fi credentials
  if (!is_incoming && !param.wifi_credentials_attachments.empty())
    return;

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  if (is_incoming) {
    for (WifiCredentialsAttachment::SecurityType securityType :
         param.wifi_credentials_attachments) {
      share_target.wifi_credentials_attachments.push_back(
          CreateWifiCredentialsAttachment(securityType));
    }
  }

  manager()->ShowSuccess(share_target);

  std::u16string expected;
  if (!param.wifi_credentials_attachments.empty() && is_incoming) {
    expected = FormatNotificationTitle(
        IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE_WIFI_CREDENTIALS, param,
        device_name, share_target.wifi_credentials_attachments[0].ssid(),
        /*use_capitalized_resource=*/true);
  } else {
    expected = FormatNotificationTitle(
        is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE
                    : IDS_NEARBY_NOTIFICATION_SEND_SUCCESS_TITLE,
        param, device_name, /*network_name=*/kWifiSsid,
        /*use_capitalized_resource=*/true);
  }

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(expected, notification.title());
}

TEST_P(NearbyNotificationManagerAttachmentsTest, ShowFailure) {
  const AttachmentsTestParamInternal& param = std::get<0>(GetParam());
  bool is_incoming = std::get<1>(GetParam());

  std::string device_name = "device";
  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.is_incoming = is_incoming;

  // We currently do not support sending Wi-Fi credentials
  if (!is_incoming && !param.wifi_credentials_attachments.empty())
    return;

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  if (is_incoming) {
    for (WifiCredentialsAttachment::SecurityType securityType :
         param.wifi_credentials_attachments) {
      share_target.wifi_credentials_attachments.push_back(
          CreateWifiCredentialsAttachment(securityType));
    }
  }

  for (std::optional<std::pair<TransferMetadata::Status, int>> error :
       std::vector<std::optional<std::pair<TransferMetadata::Status, int>>>{
           std::make_pair(TransferMetadata::Status::kNotEnoughSpace,
                          IDS_NEARBY_ERROR_NOT_ENOUGH_SPACE),
           std::make_pair(TransferMetadata::Status::kTimedOut,
                          IDS_NEARBY_ERROR_TIME_OUT),
           std::make_pair(TransferMetadata::Status::kUnsupportedAttachmentType,
                          IDS_NEARBY_ERROR_UNSUPPORTED_FILE_TYPE),
           std::make_pair(TransferMetadata::Status::kFailed, 0),
           std::nullopt,
       }) {
    if (error) {
      manager()->ShowFailure(
          share_target,
          TransferMetadataBuilder().set_status(error->first).build());
    } else {
      manager()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kInProgress)
                            .build());
      manager()->OnNearbyProcessStopped();
    }

    std::u16string expected_title;
    if (!param.wifi_credentials_attachments.empty() && is_incoming) {
      expected_title = FormatNotificationTitle(
          IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE_WIFI_CREDENTIALS, param,
          device_name, share_target.wifi_credentials_attachments[0].ssid(),
          /*use_capitalized_resource=*/false);
    } else {
      expected_title = FormatNotificationTitle(
          is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE
                      : IDS_NEARBY_NOTIFICATION_SEND_FAILURE_TITLE,
          param, device_name, /*network_name=*/kWifiSsid,
          /*use_capitalized_resource=*/false);
    }

    std::u16string expected_message =
        error && error->second ? l10n_util::GetStringUTF16(error->second)
                               : std::u16string();

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications();
    ASSERT_EQ(1u, notifications.size());

    const message_center::Notification& notification = notifications[0];
    EXPECT_EQ(expected_title, notification.title());
    EXPECT_EQ(expected_message, notification.message());

    notification_tester_->RemoveNotification(
        NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(),
        /*by_user=*/true);
  }
}

INSTANTIATE_TEST_SUITE_P(
    NearbyNotificationManagerAttachmentsTest,
    NearbyNotificationManagerAttachmentsTest,
    testing::Combine(testing::ValuesIn(kAttachmentsTestParams),
                     testing::Bool()));

TEST_P(NearbyNotificationManagerConnectionRequestTest,
       ShowConnectionRequest_ShowsNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  bool with_token = GetParam();

  std::string device_name = "device";
  std::string token = "3141";

  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.file_attachments.push_back(
      CreateFileAttachment(FileAttachment::Type::kImage));

  TransferMetadataBuilder transfer_metadata_builder;
  transfer_metadata_builder.set_status(
      TransferMetadata::Status::kAwaitingLocalConfirmation);
  if (with_token)
    transfer_metadata_builder.set_token(token);
  TransferMetadata transfer_metadata = transfer_metadata_builder.build();

  manager()->ShowConnectionRequest(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];

  const std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE);
  std::u16string plural_message = l10n_util::GetPluralStringFUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE, 1);

  std::u16string expected_message = base::ReplaceStringPlaceholders(
      plural_message,
      {base::ASCIIToUTF16(device_name),
       l10n_util::GetPluralStringFUTF16(
           IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_IMAGES, 1)},
      /*offsets=*/nullptr);

  if (with_token) {
    expected_message = base::StrCat(
        {expected_message, u"\n",
         l10n_util::GetStringFUTF16(IDS_NEARBY_SECURE_CONNECTION_ID,
                                    base::UTF8ToUTF16(token))});
  }

  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(expected_title, notification.title());
  EXPECT_EQ(expected_message, notification.message());
  // TODO(crbug.com/40138752): verify notification.icon()
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACCEPT_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i) {
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
  }
}

TEST_P(NearbyNotificationManagerConnectionRequestTest,
       ShowConnectionRequest_ShowsNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  bool with_token = GetParam();

  std::string device_name = "device";
  std::string token = "3141";

  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.file_attachments.push_back(
      CreateFileAttachment(FileAttachment::Type::kImage));

  TransferMetadataBuilder transfer_metadata_builder;
  transfer_metadata_builder.set_status(
      TransferMetadata::Status::kAwaitingLocalConfirmation);
  if (with_token) {
    transfer_metadata_builder.set_token(token);
  }
  TransferMetadata transfer_metadata = transfer_metadata_builder.build();

  manager()->ShowConnectionRequest(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];

  const std::u16string expected_title =
      NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
          IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE_PH);
  std::u16string plural_message = l10n_util::GetPluralStringFUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE, 1);

  std::u16string expected_message = base::ReplaceStringPlaceholders(
      plural_message,
      {base::ASCIIToUTF16(device_name),
       l10n_util::GetPluralStringFUTF16(
           IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_IMAGES, 1)},
      /*offsets=*/nullptr);

  if (with_token) {
    expected_message = base::StrCat(
        {expected_message, u"\n",
         l10n_util::GetStringFUTF16(IDS_NEARBY_SECURE_CONNECTION_ID,
                                    base::UTF8ToUTF16(token))});
  }

  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(expected_title, notification.title());
  EXPECT_EQ(expected_message, notification.message());
  // TODO(crbug.com/40138752): verify notification.icon()
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACCEPT_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i)
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
}

INSTANTIATE_TEST_SUITE_P(NearbyNotificationManagerConnectionRequestTest,
                         NearbyNotificationManagerConnectionRequestTest,
                         testing::Bool());

TEST_F(NearbyNotificationManagerTest,
       ShowConnectionRequest_DeviceNameEncoding) {
  ShareTarget share_target;
  share_target.device_name = "🌵";

  manager()->ShowConnectionRequest(share_target,
                                   TransferMetadataBuilder().build());
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  std::string message = base::UTF16ToUTF8(notifications[0].message());
  EXPECT_TRUE(message.find(share_target.device_name) != std::string::npos);
}

TEST_F(NearbyNotificationManagerTest,
       ShowNearbyDeviceTryingToShare_ShowsNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  manager()->ShowNearbyDeviceTryingToShare();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE),
      notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SET_UP_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i)
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
}

TEST_F(NearbyNotificationManagerTest,
       ShowNearbyDeviceTryingToShare_ShowsNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  manager()->ShowNearbyDeviceTryingToShare();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE_PH),
            notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SET_UP_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i) {
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
  }
}

TEST_F(
    NearbyNotificationManagerTest,
    ShowNearbyDeviceTryingToShare_AlreadyOnboarded_ShowsGoVisibleNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  pref_service_.SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                           true);
  manager()->ShowNearbyDeviceTryingToShare();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_VISIBLE_MESSAGE),
      notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());
  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_VISIBLE_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i) {
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
  }
}

TEST_F(
    NearbyNotificationManagerTest,
    ShowNearbyDeviceTryingToShare_AlreadyOnboarded_ShowsGoVisibleNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  pref_service_.SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                           true);
  manager()->ShowNearbyDeviceTryingToShare();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_VISIBLE_MESSAGE),
      notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());
  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_VISIBLE_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i)
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
}

TEST_F(NearbyNotificationManagerTest,
       FastInitiationDeviceFound_ShowsNearbyDeviceTryingToShare) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  manager()->OnFastInitiationDevicesDetected();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  // Minimum to confirm it's actually the onboarding notification.
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE),
      notification.message());
}

TEST_F(NearbyNotificationManagerTest,
       FastInitiationDeviceFound_ShowsNearbyDeviceTryingToShare_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  manager()->OnFastInitiationDevicesDetected();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  // Minimum to confirm it's actually the onboarding notification.
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE_PH),
            notification.message());
}

TEST_F(NearbyNotificationManagerTest,
       FastInitiationDeviceLost_ClosesNearbyDeviceTryingToShare) {
  manager()->OnFastInitiationDevicesDetected();
  EXPECT_EQ(1u, GetDisplayedNotifications().size());

  manager()->OnFastInitiationDevicesNotDetected();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       FastInitiationScanningStopped_ClosesNearbyDeviceTryingToShare) {
  manager()->OnFastInitiationDevicesDetected();
  EXPECT_EQ(1u, GetDisplayedNotifications().size());

  manager()->OnFastInitiationScanningStopped();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ShowSuccess_ShowsNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  manager()->ShowSuccess(ShareTarget());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest,
       ShowSuccess_ShowsNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  manager()->ShowSuccess(ShareTarget());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest, ShowSuccess_DeviceNameEncoding) {
  ShareTarget share_target;
  share_target.device_name = "🌵";

  manager()->ShowSuccess(share_target);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  std::string title = base::UTF16ToUTF8(notifications[0].title());
  EXPECT_TRUE(title.find(share_target.device_name) != std::string::npos);
}

TEST_F(NearbyNotificationManagerTest, ShowCancelled_ShowsNotification) {
  ShareTarget share_target;
  manager()->ShowCancelled(share_target);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
}

TEST_F(NearbyNotificationManagerTest, ShowCancelled_DeviceNameEncoding) {
  ShareTarget share_target;
  share_target.device_name = "🌵";

  manager()->ShowCancelled(share_target);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  std::string title = base::UTF16ToUTF8(notifications[0].title());
  EXPECT_TRUE(title.find(share_target.device_name) != std::string::npos);
}

TEST_F(NearbyNotificationManagerTest, ShowFailure_ShowsNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  manager()->ShowFailure(ShareTarget(), TransferMetadataBuilder().build());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest,
       ShowFailure_ShowsNotification_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  manager()->ShowFailure(ShareTarget(), TransferMetadataBuilder().build());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(std::u16string(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest, ShowFailure_DeviceNameEncoding) {
  ShareTarget share_target;
  share_target.device_name = "🌵";

  manager()->ShowFailure(share_target, TransferMetadataBuilder().build());
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  std::string title = base::UTF16ToUTF8(notifications[0].title());
  EXPECT_TRUE(title.find(share_target.device_name) != std::string::npos);
}

TEST_F(NearbyNotificationManagerTest,
       CloseTransfer_ClosesProgressNotification) {
  manager()->ShowProgress(ShareTarget(), TransferMetadataBuilder().build());
  ASSERT_EQ(1u, GetDisplayedNotifications().size());

  manager()->CloseTransfer();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       CloseTransfer_ClosesConnectionNotification) {
  manager()->ShowConnectionRequest(ShareTarget(),
                                   TransferMetadataBuilder().build());
  ASSERT_EQ(1u, GetDisplayedNotifications().size());

  manager()->CloseTransfer();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       CloseProgressNotification_NoopWithoutNotification) {
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  manager()->CloseTransfer();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       CloseProgressNotification_KeepsNearbyDeviceTryingToShareNotification) {
  manager()->ShowNearbyDeviceTryingToShare();

  manager()->CloseTransfer();
  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ProgressNotification_Cancel) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build();

  // Simulate incoming transfer progress.
  manager()->OnTransferUpdate(share_target, transfer_metadata);

  // Expect a notification with a cancel button.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(1u, notifications[0].buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL),
            notifications[0].buttons()[0].title);

  // Expect call to Cancel on button click.
  EXPECT_CALL(*nearby_service_,
              Cancel(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  // Notification should be closed on button click.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ProgressNotification_Close) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build();

  // Simulate incoming transfer progress.
  manager()->OnTransferUpdate(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  // Expect call to Cancel on notification close.
  EXPECT_CALL(*nearby_service_,
              Cancel(MatchesTarget(share_target), testing::_));
  notification_tester_->RemoveNotification(
      NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(),
      /*by_user=*/true);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ProgressNotification_Cancelled) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build();

  // Simulate incoming transfer progress.
  manager()->OnTransferUpdate(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  // Simulate cancelled transfer.
  manager()->OnTransferUpdate(
      share_target, TransferMetadataBuilder()
                        .set_status(TransferMetadata::Status::kCancelled)
                        .build());

  // Cancelled notification should be shown.
  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ConnectionRequest_Accept) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .build();

  // Simulate incoming connection request waiting for local confirmation.
  manager()->OnTransferUpdate(share_target, transfer_metadata);

  // Expect a notification with an accept button.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(2u, notifications[0].buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACCEPT_ACTION),
            notifications[0].buttons()[0].title);

  // Expect call to Accept on button click.
  EXPECT_CALL(*nearby_service_,
              Accept(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  // Notification should still be present as it will soon be replaced.
  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ConnectionRequest_Reject_Local) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .build();

  // Simulate incoming connection request waiting for local confirmation.
  manager()->OnTransferUpdate(share_target, transfer_metadata);

  // Expect a notification with a reject button.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(2u, notifications[0].buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION),
            notifications[0].buttons()[1].title);

  // Expect call to Reject on button click.
  EXPECT_CALL(*nearby_service_,
              Reject(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/1,
                                      /*reply=*/std::nullopt);

  // Notification should be closed on button click.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ProgressNotification_Reject_Remote) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build();

  // Simulate incoming connection request waiting for remote acceptance.
  manager()->OnTransferUpdate(share_target, transfer_metadata);

  // Expect a notification with only the reject button.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(1u, notifications[0].buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL),
            notifications[0].buttons()[0].title);

  // Expect call to Reject on button click.
  EXPECT_CALL(*nearby_service_,
              Reject(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  // Notification should be closed on button click.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ConnectionRequest_Close) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .build();

  // Simulate incoming connection request waiting for local confirmation.
  manager()->OnTransferUpdate(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  // Expect call to Reject on notification close.
  EXPECT_CALL(*nearby_service_,
              Reject(MatchesTarget(share_target), testing::_));
  notification_tester_->RemoveNotification(
      NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(),
      /*by_user=*/true);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, NearbyDeviceTryingToShare_Click) {
  manager()->ShowNearbyDeviceTryingToShare();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_, _));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       NearbyDeviceTryingToShare_OnClose_DismissTimeout) {
  // First notification should be shown by default.
  manager()->ShowNearbyDeviceTryingToShare();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification_tester_->RemoveNotification(
      NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(),
      /*by_user=*/true);
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Second notification should be blocked if shown before the timeout passed.
  manager()->ShowNearbyDeviceTryingToShare();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Fast forward by the timeout until we can show the notification again.
  task_environment_.FastForwardBy(
      NearbyNotificationManager::kNearbyDeviceTryingToShareDismissedTimeout);
  manager()->ShowNearbyDeviceTryingToShare();
  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       NearbyDeviceTryingToShare_OnDismissClicked_DismissTimeout) {
  // First notification should be shown by default.
  manager()->ShowNearbyDeviceTryingToShare();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/1,
                                      /*reply=*/std::nullopt);
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Second notification should be blocked if shown before the timeout passed.
  manager()->ShowNearbyDeviceTryingToShare();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Fast forward by the timeout until we can show the notification again.
  task_environment_.FastForwardBy(
      NearbyNotificationManager::kNearbyDeviceTryingToShareDismissedTimeout);
  manager()->ShowNearbyDeviceTryingToShare();
  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_SingleImageReceived_OpenDownloads) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(NearbyNotificationManager::SuccessNotificationAction::
                      kOpenDownloads,
                  action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/1,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  // Image decoding happens asynchronously so wait for the notification to show.
  base::RunLoop display_run_loop;
  notification_tester_->SetNotificationAddedClosure(
      display_run_loop.QuitClosure());
  display_run_loop.Run();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());
  EXPECT_FALSE(notification.image().IsEmpty());
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER),
      notification.buttons()[0].title);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD),
            notification.buttons()[1].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_SingleImageReceived_CopyToClipboard) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(
            NearbyNotificationManager::SuccessNotificationAction::kCopyImage,
            action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/1,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  // Image decoding happens asynchronously so wait for the notification to show.
  base::RunLoop display_run_loop;
  notification_tester_->SetNotificationAddedClosure(
      display_run_loop.QuitClosure());
  display_run_loop.Run();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());
  EXPECT_FALSE(notification.image().IsEmpty());
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER),
      notification.buttons()[0].title);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD),
            notification.buttons()[1].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/1,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Expected behaviour is to copy to clipboard.
  SkBitmap image = GetClipboardImage();
  EXPECT_TRUE(gfx::BitmapsAreEqual(CreateTestSkBitmap(), image));

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_MultipleImagesReceived) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(NearbyNotificationManager::SuccessNotificationAction::
                      kOpenDownloads,
                  action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/2,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_TRUE(notification.image().IsEmpty());
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER),
      notification.buttons()[0].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, SuccessNotificationClicked_TextReceived) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(
            NearbyNotificationManager::SuccessNotificationAction::kCopyText,
            action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/1, /*url_attachments=*/0, /*image_attachments=*/0,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD),
            notification.buttons()[0].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();
  EXPECT_EQ(kTextBody, GetClipboardText());

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_UrlReceived_OpenUrl) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(
            NearbyNotificationManager::SuccessNotificationAction::kOpenUrl,
            action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/1, /*image_attachments=*/0,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_URL),
            notification.buttons()[0].title);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD),
            notification.buttons()[1].title);

  EXPECT_CALL(*nearby_service_, OpenURL(testing::_)).Times(1);
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_UrlReceived_CopyToClipboard) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(
            NearbyNotificationManager::SuccessNotificationAction::kCopyText,
            action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/1, /*image_attachments=*/0,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_URL),
            notification.buttons()[0].title);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD),
            notification.buttons()[1].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/1,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Expected behaviour is to copy to clipboard.
  EXPECT_EQ(kTextUrl, GetClipboardText());

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_SingleFileReceived) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(NearbyNotificationManager::SuccessNotificationAction::
                      kOpenDownloads,
                  action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/0,
      /*other_file_attachments=*/1, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER),
      notification.buttons()[0].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_MultipleFilesReceived) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(NearbyNotificationManager::SuccessNotificationAction::
                      kOpenDownloads,
                  action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/1,
      /*other_file_attachments=*/2, /*wifi_credentials_attachments=*/false);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER),
      notification.buttons()[0].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       SuccessNotificationClicked_WifiCredentialsReceived) {
  base::RunLoop run_loop;
  manager()->SetOnSuccessClickedForTesting(base::BindLambdaForTesting(
      [&](NearbyNotificationManager::SuccessNotificationAction action) {
        EXPECT_EQ(NearbyNotificationManager::SuccessNotificationAction::
                      kOpenWifiNetworksList,
                  action);
        run_loop.Quit();
      }));

  ShareTarget share_target = CreateIncomingShareTarget(
      /*text_attachments=*/0, /*url_attachments=*/0, /*image_attachments=*/0,
      /*other_file_attachments=*/0, /*wifi_credentials_attachments=*/true);
  manager()->ShowSuccess(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ACTION_OPEN_NETWORK_LIST),
            notification.buttons()[0].title);

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notification.id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

class NearbyFilesHoldingSpaceTest : public testing::Test {
 public:
  NearbyFilesHoldingSpaceTest()
      : session_controller_(std::make_unique<TestSessionController>()),
        user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);

    holding_space_controller_ = std::make_unique<ash::HoldingSpaceController>();
    profile_manager_ = CreateTestingProfileManager();
    constexpr char kEmail[] = "test@test";
    const AccountId account_id(AccountId::FromUserEmail(kEmail));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    profile_ = profile_manager_->CreateTestingProfile(kEmail);
    static_cast<ash::SessionObserver*>(holding_space_controller_.get())
        ->OnActiveUserSessionChanged(account_id);
  }

  ~NearbyFilesHoldingSpaceTest() override = default;

  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<NearbyNotificationManager>(
        NotificationDisplayServiceFactory::GetForProfile(profile_),
        CreateAndUseMockNearbySharingService(profile_), profile_->GetPrefs(),
        profile_);
  }

  NearbyNotificationManager* manager() { return manager_.get(); }

  ash::HoldingSpaceModel* GetHoldingSpaceModel() const {
    return holding_space_controller_ ? holding_space_controller_->model()
                                     : nullptr;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<NearbyNotificationManager> manager_;
  std::unique_ptr<TestSessionController> session_controller_;
  std::unique_ptr<ash::HoldingSpaceController> holding_space_controller_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
};

TEST_F(NearbyFilesHoldingSpaceTest, ShowSuccess_Files) {
  std::unique_ptr<ash::holding_space::ScopedTestMountPoint> downloads_mount =
      ash::holding_space::ScopedTestMountPoint::CreateAndMountDownloads(
          profile_);
  ASSERT_TRUE(downloads_mount->IsValid());

  ShareTarget share_target;
  share_target.is_incoming = true;

  const base::FilePath file_virtual_path("Sample.txt");
  base::FilePath file_path =
      downloads_mount->CreateFile(file_virtual_path, "Sample Text");

  FileAttachment attachment(file_path);
  share_target.file_attachments.push_back(std::move(attachment));

  manager()->ShowSuccess(share_target);

  ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
  ASSERT_TRUE(holding_space_model);

  ASSERT_EQ(share_target.file_attachments.size(),
            holding_space_model->items().size());

  for (size_t i = 0; i < share_target.file_attachments.size(); ++i) {
    ash::HoldingSpaceItem* holding_space_item =
        holding_space_model->items()[i].get();
    EXPECT_EQ(ash::HoldingSpaceItem::Type::kNearbyShare,
              holding_space_item->type());

    EXPECT_EQ(share_target.file_attachments[i].file_path(),
              holding_space_item->file().file_path);
  }
}

TEST_F(NearbyFilesHoldingSpaceTest, ShowSuccess_Text) {
  ShareTarget share_target;
  share_target.is_incoming = true;

  TextAttachment attachment(TextAttachment::Type::kText, "Sample Text",
                            /*title=*/std::nullopt,
                            /*mime_type=*/std::nullopt);
  share_target.text_attachments.push_back(std::move(attachment));

  manager()->ShowSuccess(share_target);

  ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
  ASSERT_TRUE(holding_space_model);

  EXPECT_TRUE(holding_space_model->items().empty());
}

TEST_F(NearbyFilesHoldingSpaceTest, ShowSuccess_WifiCredentials) {
  ShareTarget share_target;
  share_target.is_incoming = true;

  WifiCredentialsAttachment attachment(kWifiCredentialsId, kSecurityType,
                                       kWifiSsid);
  share_target.wifi_credentials_attachments.push_back(std::move(attachment));

  manager()->ShowSuccess(share_target);

  ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
  ASSERT_TRUE(holding_space_model);

  EXPECT_TRUE(holding_space_model->items().empty());
}

TEST_F(NearbyNotificationManagerTest, ShowMultipleNotifications) {
  // Show two of each type of completion notification and ensure all are
  // displayed. We also show a progress notification to ensure it doesn't
  // interfere with the others.
  ShareTarget share_target_a;
  manager()->ShowSuccess(share_target_a);
  manager()->ShowSuccess(ShareTarget());
  manager()->ShowFailure(ShareTarget(), TransferMetadataBuilder().build());
  manager()->ShowFailure(ShareTarget(), TransferMetadataBuilder().build());
  manager()->ShowCancelled(ShareTarget());
  manager()->ShowCancelled(ShareTarget());
  manager()->ShowProgress(ShareTarget(), TransferMetadataBuilder().build());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(7u, notifications.size());

  // Make sure we can close an individual one.
  manager()->CloseSuccessNotification("chrome://nearby_share/result/" +
                                      share_target_a.id.ToString());
  notifications = GetDisplayedNotifications();
  ASSERT_EQ(6u, notifications.size());
}

TEST_F(NearbyNotificationManagerTest, ShowVisibilityReminder_Contacts_Mode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kIsNameEnabled});
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_TITLE),
            notification.title());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_MESSAGE),
            notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_TO_SETTINGS_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i) {
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
  }
}

TEST_F(NearbyNotificationManagerTest,
       ShowVisibilityReminder_Contacts_Mode_NameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kIsNameEnabled},
      /*disabled_features=*/{});
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_TITLE),
            notification.title());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_MESSAGE),
            notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareInternalIcon, &notification.vector_small_image());
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH),
            notification.display_source());
  EXPECT_EQ(2u, notification.buttons().size());

  std::vector<std::u16string> expected_button_titles;
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_TO_SETTINGS_ACTION));
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i) {
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
  }
}

TEST_F(NearbyNotificationManagerTest, ShowVisibilityReminder_Hidden_Mode) {
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(0u, notifications.size());
}

TEST_F(NearbyNotificationManagerTest,
       ShowVisibilityReminder_Notification_Clicked) {
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kSelectedContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_, _));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/std::optional<int>(),
                                      /*reply=*/std::nullopt);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ShowVisibilityReminder_Settings_Clicked) {
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_, _));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/0,
                                      /*reply=*/std::nullopt);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ShowVisibilityReminder_Dismiss_Clicked) {
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/1,
                                      /*reply=*/std::nullopt);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest,
       ShowVisibilityReminder_Notification_Closed) {
  pref_service_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(nearby_share::mojom::Visibility::kAllContacts));

  manager()->ShowVisibilityReminder();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());
  notification_tester_->RemoveNotification(
      NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(), true);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ConnectionRequest_SelfShare) {
  ShareTarget share_target;
  share_target.is_incoming = true;
  share_target.for_self_share = true;
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .build();

  // Simulate incoming connection request waiting for local confirmation.
  manager()->OnTransferUpdate(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(0u, notifications.size());
}

TEST_F(NearbyNotificationManagerTest,
       ConnectionRequest_SelfShare_WiFiCantAutoAccept) {
  ShareTarget share_target;
  // Incoming Wi-Fi credential Self Share.
  share_target.is_incoming = true;
  share_target.for_self_share = true;
  share_target.wifi_credentials_attachments.push_back(
      CreateWifiCredentialsAttachment(
          WifiCredentialsAttachment::SecurityType::kWpaPsk));
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .build();

  // Simulate incoming connection request waiting for local confirmation.
  manager()->OnTransferUpdate(share_target, transfer_metadata);
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  // We can't auto-accept Wi-Fi credentials, so expect the confirmation
  // notification whether or not self share is enabled.
  ASSERT_EQ(1u, notifications.size());
}
