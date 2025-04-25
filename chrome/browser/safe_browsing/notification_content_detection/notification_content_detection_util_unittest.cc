// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_util.h"

#include "base/json/json_string_value_serializer.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"

namespace safe_browsing {

class NotificationContentDetectionUtilTest : public testing::Test {
 public:
  NotificationContentDetectionUtilTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());

    profile_ = manager_.CreateTestingProfile("foo");
    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockOptimizationGuideKeyedService>();
                })));
    auto logs_uploader = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        manager_.local_state()->Get());
    mock_optimization_guide_keyed_service_
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));
  }

  optimization_guide::TestModelQualityLogsUploaderService* logs_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        mock_optimization_guide_keyed_service_
            ->GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return logs_uploader()->uploaded_logs();
  }

  void CheckLoggedNotificationContents(
      optimization_guide::proto::NotificationContents
          logged_notification_contents,
      std::string expected_title,
      std::string expected_body,
      std::string expected_origin_str) {
    ASSERT_EQ(expected_title,
              logged_notification_contents.notification_title());
    ASSERT_EQ(expected_body,
              logged_notification_contents.notification_message());
    ASSERT_EQ(expected_origin_str, logged_notification_contents.url());
  }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
};

TEST_F(NotificationContentDetectionUtilTest, TestLoggingWithValidMetadata) {
  std::u16string title = u"Notification title";
  std::u16string body = u"Notification body";
  GURL origin = GURL("example.com");
  float suspicious_score = 70.0;

  content::NotificationDatabaseData database_data;
  database_data.notification_data.title = title;
  database_data.notification_data.body = body;
  database_data.origin = origin;
  database_data.serialized_metadata[safe_browsing::kMetadataDictionaryKey] =
      NotificationContentDetectionModel::GetSerializedMetadata(
          false, false, suspicious_score);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  SendNotificationContentDetectionDataToMQLSServer(
      logs_uploader()->GetWeakPtr(),
      NotificationContentDetectionMQLSMetadata(
          true, true, blink::mojom::EngagementLevel::MEDIUM),
      /*success=*/true, database_data);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  CheckLoggedNotificationContents(
      notification_content_detection->request().notification_contents(),
      base::UTF16ToUTF8(title), base::UTF16ToUTF8(body), origin.spec());

  // Check logged metadata.
  ASSERT_EQ(suspicious_score,
            notification_content_detection->response().suspicious_score());
  ASSERT_FALSE(notification_content_detection->quality().is_url_on_allowlist());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_always_allow_url());
  ASSERT_TRUE(
      notification_content_detection->quality().was_user_shown_warning());
  ASSERT_TRUE(notification_content_detection->quality().did_user_unsubscribe());
  ASSERT_EQ(optimization_guide::proto::SiteEngagementScore::
                SITE_ENGAGEMENT_SCORE_MEDIUM,
            notification_content_detection->quality().site_engagement_score());
}

TEST_F(NotificationContentDetectionUtilTest,
       TestLoggingWithValidMetadataNoSuspiciousScore) {
  std::u16string title = u"Notification title";
  std::u16string body = u"Notification body";
  GURL origin = GURL("example.com");

  content::NotificationDatabaseData database_data;
  database_data.notification_data.title = title;
  database_data.notification_data.body = body;
  database_data.origin = origin;
  database_data.serialized_metadata[safe_browsing::kMetadataDictionaryKey] =
      NotificationContentDetectionModel::GetSerializedMetadata(false, false,
                                                               std::nullopt);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  SendNotificationContentDetectionDataToMQLSServer(
      logs_uploader()->GetWeakPtr(),
      NotificationContentDetectionMQLSMetadata(
          true, true, blink::mojom::EngagementLevel::MEDIUM),
      /*success=*/true, database_data);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  // Check logged metadata.
  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  ASSERT_FALSE(notification_content_detection->quality().is_url_on_allowlist());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_always_allow_url());
}

TEST_F(NotificationContentDetectionUtilTest,
       TestLoggingWithNullSerializedMetadata) {
  std::u16string title = u"Notification title";
  std::u16string body = u"Notification body";
  GURL origin = GURL("example.com");

  content::NotificationDatabaseData database_data;
  database_data.notification_data.title = title;
  database_data.notification_data.body = body;
  database_data.origin = origin;

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  SendNotificationContentDetectionDataToMQLSServer(
      logs_uploader()->GetWeakPtr(),
      NotificationContentDetectionMQLSMetadata(
          true, false, blink::mojom::EngagementLevel::NONE),
      /*success=*/true, database_data);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  CheckLoggedNotificationContents(
      notification_content_detection->request().notification_contents(),
      base::UTF16ToUTF8(title), base::UTF16ToUTF8(body), origin.spec());
}

TEST_F(NotificationContentDetectionUtilTest,
       TestLoggingWithInvalidSerializedMetadata) {
  std::u16string title = u"Notification title";
  std::u16string body = u"Notification body";
  GURL origin = GURL("example.com");

  content::NotificationDatabaseData database_data;
  database_data.notification_data.title = title;
  database_data.notification_data.body = body;
  database_data.origin = origin;
  database_data.serialized_metadata[safe_browsing::kMetadataDictionaryKey] =
      "Invalid";

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  SendNotificationContentDetectionDataToMQLSServer(
      logs_uploader()->GetWeakPtr(),
      NotificationContentDetectionMQLSMetadata(
          false, true, blink::mojom::EngagementLevel::HIGH),
      /*success=*/true, database_data);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  CheckLoggedNotificationContents(
      notification_content_detection->request().notification_contents(),
      base::UTF16ToUTF8(title), base::UTF16ToUTF8(body), origin.spec());
}

TEST_F(NotificationContentDetectionUtilTest, TestNoLoggingWhenSuccessFalse) {
  std::u16string title = u"Notification title";
  std::u16string body = u"Notification body";
  GURL origin = GURL("example.com");
  float suspicious_score = 70.0;

  content::NotificationDatabaseData database_data;
  database_data.notification_data.title = title;
  database_data.notification_data.body = body;
  database_data.origin = origin;
  database_data.serialized_metadata[safe_browsing::kMetadataDictionaryKey] =
      NotificationContentDetectionModel::GetSerializedMetadata(
          false, false, suspicious_score);

  SendNotificationContentDetectionDataToMQLSServer(
      logs_uploader()->GetWeakPtr(),
      NotificationContentDetectionMQLSMetadata(
          true, true, blink::mojom::EngagementLevel::MEDIUM),
      /*success=*/false, database_data);
  const auto& logs = uploaded_logs();
  ASSERT_EQ(0u, logs.size());
}

}  // namespace safe_browsing
