// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const char kTextBody[] = "text body";

MATCHER_P(MatchesTarget, target, "") {
  return arg.id == target.id;
}

TextAttachment CreateTextAttachment(TextAttachment::Type type) {
  return TextAttachment(type, kTextBody);
}

FileAttachment CreateFileAttachment(FileAttachment::Type type) {
  return FileAttachment(/*id=*/0, /*size=*/10, /*file_name=*/"file.jpg",
                        /*mime_type=*/"example", type);
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
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
  return base::UTF16ToUTF8(text);
}

SkBitmap GetClipboardImage() {
  return ui::clipboard_test_util::ReadImage(
      ui::Clipboard::GetForCurrentThread());
}

SkBitmap CreateTestSkBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(/*w=*/10, /*h=*/15);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

class NearbyNotificationManagerTest : public testing::Test {
 public:
  NearbyNotificationManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);
    RegisterNearbySharingPrefs(pref_service_.registry());
  }

  ~NearbyNotificationManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);
    nearby_service_ = CreateAndUseMockNearbySharingService(&profile_);
    manager_ = CreateManager();
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
                                        int image_attachments,
                                        int other_file_attachments) {
    ShareTarget share_target;
    share_target.is_incoming = true;
    for (int i = 0; i < text_attachments; i++) {
      share_target.text_attachments.push_back(
          CreateTextAttachment(TextAttachment::Type::kText));
    }

    for (int i = 0; i < image_attachments; i++) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      std::string name = base::StrCat({base::NumberToString(i), ".png"});
      base::FilePath file_path = temp_dir_.GetPath().AppendASCII(name);
      SkBitmap image = CreateTestSkBitmap();

      std::vector<unsigned char> png_data;
      EXPECT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(
          image, /*discard_transparency=*/true, &png_data));
      char* data = reinterpret_cast<char*>(&png_data[0]);
      int size = static_cast<int>(png_data.size());
      base::WriteFile(file_path, data, size);

      FileAttachment attachment(file_path);
      share_target.file_attachments.push_back(std::move(attachment));
    }

    for (int i = 0; i < other_file_attachments; i++) {
      share_target.file_attachments.push_back(
          CreateFileAttachment(FileAttachment::Type::kVideo));
    }
    return share_target;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  MockNearbySharingService* nearby_service_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<NearbyNotificationManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

struct AttachmentsTestParamInternal {
  std::vector<TextAttachment::Type> text_attachments;
  std::vector<FileAttachment::Type> file_attachments;
  int expected_resource_id;
};

AttachmentsTestParamInternal kAttachmentsTestParams[] = {
    // No attachments.
    {{}, {}, IDS_NEARBY_UNKNOWN_ATTACHMENTS},

    // Mixed attachments.
    {{TextAttachment::Type::kText},
     {FileAttachment::Type::kUnknown},
     IDS_NEARBY_UNKNOWN_ATTACHMENTS},

    // Text attachments.
    {{TextAttachment::Type::kUrl}, {}, IDS_NEARBY_TEXT_ATTACHMENTS_LINKS},
    {{TextAttachment::Type::kText}, {}, IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN},
    {{TextAttachment::Type::kAddress},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES},
    {{TextAttachment::Type::kPhoneNumber},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_PHONE_NUMBERS},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kAddress},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kUrl},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN},

    // File attachments.
    {{}, {FileAttachment::Type::kApp}, IDS_NEARBY_FILE_ATTACHMENTS_APPS},
    {{}, {FileAttachment::Type::kImage}, IDS_NEARBY_FILE_ATTACHMENTS_IMAGES},
    {{}, {FileAttachment::Type::kUnknown}, IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN},
    {{}, {FileAttachment::Type::kVideo}, IDS_NEARBY_FILE_ATTACHMENTS_VIDEOS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kApp},
     IDS_NEARBY_FILE_ATTACHMENTS_APPS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kImage},
     IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN},
};

using AttachmentsTestParam = std::tuple<AttachmentsTestParamInternal, bool>;

class NearbyNotificationManagerAttachmentsTest
    : public NearbyNotificationManagerTest,
      public testing::WithParamInterface<AttachmentsTestParam> {};

using ConnectionRequestTestParam = std::tuple<TransferMetadata::Status, bool>;

class NearbyNotificationManagerConnectionRequestTest
    : public NearbyNotificationManagerTest,
      public testing::WithParamInterface<ConnectionRequestTestParam> {};

base::string16 FormatNotificationTitle(
    int resource_id,
    const AttachmentsTestParamInternal& param,
    const std::string& device_name) {
  size_t total = param.text_attachments.size() + param.file_attachments.size();
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(resource_id, total),
      {l10n_util::GetPluralStringFUTF16(param.expected_resource_id, total),
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
  ShareTarget share_target;
  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();

  manager()->ShowProgress(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());
  EXPECT_EQ(base::string16(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
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

TEST_P(NearbyNotificationManagerAttachmentsTest, ShowProgress) {
  const AttachmentsTestParamInternal& param = std::get<0>(GetParam());
  bool is_incoming = std::get<1>(GetParam());

  std::string device_name = "device";
  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.is_incoming = is_incoming;

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();
  manager()->ShowProgress(share_target, transfer_metadata);

  base::string16 expected = FormatNotificationTitle(
      is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
                  : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE,
      param, device_name);

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

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  manager()->ShowSuccess(share_target);

  base::string16 expected = FormatNotificationTitle(
      is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE
                  : IDS_NEARBY_NOTIFICATION_SEND_SUCCESS_TITLE,
      param, device_name);

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

  for (TextAttachment::Type type : param.text_attachments)
    share_target.text_attachments.push_back(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target.file_attachments.push_back(CreateFileAttachment(type));

  manager()->ShowFailure(share_target);

  base::string16 expected = FormatNotificationTitle(
      is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE
                  : IDS_NEARBY_NOTIFICATION_SEND_FAILURE_TITLE,
      param, device_name);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(expected, notification.title());
}

INSTANTIATE_TEST_SUITE_P(
    NearbyNotificationManagerAttachmentsTest,
    NearbyNotificationManagerAttachmentsTest,
    testing::Combine(testing::ValuesIn(kAttachmentsTestParams),
                     testing::Bool()));

TEST_P(NearbyNotificationManagerConnectionRequestTest,
       ShowConnectionRequest_ShowsNotification) {
  TransferMetadata::Status status = std::get<0>(GetParam());
  bool with_token = std::get<1>(GetParam());

  std::string device_name = "device";
  std::string token = "3141";

  ShareTarget share_target;
  share_target.device_name = device_name;
  share_target.file_attachments.push_back(
      CreateFileAttachment(FileAttachment::Type::kImage));

  TransferMetadataBuilder transfer_metadata_builder;
  transfer_metadata_builder.set_status(status);
  if (with_token)
    transfer_metadata_builder.set_token(token);
  TransferMetadata transfer_metadata = transfer_metadata_builder.build();

  manager()->ShowConnectionRequest(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];

  base::string16 expected_title = l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE);
  base::string16 plural_message = l10n_util::GetPluralStringFUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE, 1);

  base::string16 expected_message = base::ReplaceStringPlaceholders(
      plural_message,
      {base::ASCIIToUTF16(device_name),
       l10n_util::GetPluralStringFUTF16(IDS_NEARBY_FILE_ATTACHMENTS_IMAGES, 1)},
      /*offsets=*/nullptr);

  if (with_token) {
    expected_message = base::StrCat(
        {expected_message, base::UTF8ToUTF16("\n"),
         l10n_util::GetStringFUTF16(IDS_NEARBY_SECURE_CONNECTION_ID,
                                    base::UTF8ToUTF16(token))});
  }

  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(expected_title, notification.title());
  EXPECT_EQ(expected_message, notification.message());
  // TODO(crbug.com/1102348): verify notification.icon()
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());

  std::vector<base::string16> expected_button_titles;
  if (status == TransferMetadata::Status::kAwaitingLocalConfirmation) {
    expected_button_titles.push_back(
        l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_RECEIVE_ACTION));
  }
  expected_button_titles.push_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION));

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(expected_button_titles.size(), buttons.size());

  for (size_t i = 0; i < expected_button_titles.size(); ++i)
    EXPECT_EQ(expected_button_titles[i], buttons[i].title);
}

INSTANTIATE_TEST_SUITE_P(
    NearbyNotificationManagerConnectionRequestTest,
    NearbyNotificationManagerConnectionRequestTest,
    testing::Combine(
        testing::Values(TransferMetadata::Status::kAwaitingLocalConfirmation,
                        TransferMetadata::Status::kAwaitingRemoteAcceptance),
        testing::Bool()));

TEST_F(NearbyNotificationManagerTest, ShowOnboarding_ShowsNotification) {
  manager()->ShowOnboarding();

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
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest, ShowSuccess_ShowsNotification) {
  manager()->ShowSuccess(ShareTarget());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(base::string16(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}

TEST_F(NearbyNotificationManagerTest, ShowFailure_ShowsNotification) {
  manager()->ShowFailure(ShareTarget());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  EXPECT_EQ(base::string16(), notification.message());
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
       CloseProgressNotification_KeepsOnboardingNotification) {
  manager()->ShowOnboarding();

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
                                      /*reply=*/base::nullopt);

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

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
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
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_RECEIVE_ACTION),
            notifications[0].buttons()[0].title);

  // Expect call to Accept on button click.
  EXPECT_CALL(*nearby_service_,
              Accept(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/0,
                                      /*reply=*/base::nullopt);

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
                                      /*reply=*/base::nullopt);

  // Notification should be closed on button click.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, ConnectionRequest_Reject_Remote) {
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
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION),
            notifications[0].buttons()[0].title);

  // Expect call to Reject on button click.
  EXPECT_CALL(*nearby_service_,
              Reject(MatchesTarget(share_target), testing::_));
  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(), /*action_index=*/0,
                                      /*reply=*/base::nullopt);

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

TEST_F(NearbyNotificationManagerTest, Onboarding_Click) {
  manager()->ShowOnboarding();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification_tester_->SimulateClick(NotificationHandler::Type::NEARBY_SHARE,
                                      notifications[0].id(),
                                      /*action_index=*/base::nullopt,
                                      /*reply=*/base::nullopt);

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(NearbyNotificationManagerTest, Onboarding_DismissTimeout) {
  // First notification should be shown by default.
  manager()->ShowOnboarding();
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification_tester_->RemoveNotification(
      NotificationHandler::Type::NEARBY_SHARE, notifications[0].id(),
      /*by_user=*/true);
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Second notification should be blocked if shown before the timeout passed.
  manager()->ShowOnboarding();
  EXPECT_EQ(0u, GetDisplayedNotifications().size());

  // Fast forward by the timeout until we can show the notification again.
  task_environment_.FastForwardBy(
      NearbyNotificationManager::kOnboardingDismissedTimeout);
  manager()->ShowOnboarding();
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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/0, /*image_attachments=*/1,
                                /*other_file_attachments=*/0);
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
                                      /*reply=*/base::nullopt);

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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/0, /*image_attachments=*/1,
                                /*other_file_attachments=*/0);
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
                                      /*reply=*/base::nullopt);

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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/0, /*image_attachments=*/2,
                                /*other_file_attachments=*/0);
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
                                      /*reply=*/base::nullopt);

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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/1, /*image_attachments=*/0,
                                /*other_file_attachments=*/0);
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
                                      /*reply=*/base::nullopt);

  run_loop.Run();
  EXPECT_EQ(kTextBody, GetClipboardText());

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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/0, /*image_attachments=*/0,
                                /*other_file_attachments=*/1);
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
                                      /*reply=*/base::nullopt);

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

  ShareTarget share_target =
      CreateIncomingShareTarget(/*text_attachments=*/0, /*image_attachments=*/1,
                                /*other_file_attachments=*/2);
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
                                      /*reply=*/base::nullopt);

  run_loop.Run();

  // Notification should be closed.
  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}
