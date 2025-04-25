// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_util.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "content/public/browser/notification_database_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"

namespace {

optimization_guide::proto::SiteEngagementScore EngagementLevelToProtoScore(
    blink::mojom::EngagementLevel engagement_level) {
  switch (engagement_level) {
    case blink::mojom::EngagementLevel::NONE:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_NONE;
    case blink::mojom::EngagementLevel::MINIMAL:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_MINIMAL;
    case blink::mojom::EngagementLevel::LOW:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_LOW;
    case blink::mojom::EngagementLevel::MEDIUM:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_MEDIUM;
    case blink::mojom::EngagementLevel::HIGH:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_HIGH;
    case blink::mojom::EngagementLevel::MAX:
      return optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_MAX;
  }
  NOTREACHED();
}

}  // namespace

namespace safe_browsing {

NotificationContentDetectionMQLSMetadata::
    NotificationContentDetectionMQLSMetadata(
        bool did_show_warning,
        bool did_user_unsubscribe,
        blink::mojom::EngagementLevel site_engagement_score)
    : did_show_warning_(did_show_warning),
      did_user_unsubscribe_(did_user_unsubscribe),
      site_engagement_score_(site_engagement_score) {}

void SendNotificationContentDetectionDataToMQLSServer(
    base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService>
        logs_uploader_service,
    NotificationContentDetectionMQLSMetadata mqls_metadata,
    bool success,
    const content::NotificationDatabaseData& notification_database_data) {
  if (!success) {
    return;
  }
  // Create `NotificationContents`, for logging.
  auto notification_contents =
      std::make_unique<optimization_guide::proto::NotificationContents>();
  notification_contents->set_notification_title(
      base::UTF16ToUTF8(notification_database_data.notification_data.title));
  notification_contents->set_notification_message(
      base::UTF16ToUTF8(notification_database_data.notification_data.body));
  for (const auto& action :
       notification_database_data.notification_data.actions) {
    notification_contents->add_notification_action_labels(
        base::UTF16ToUTF8(action->title));
  }
  notification_contents->set_url(notification_database_data.origin.spec());

  // Create `NotificationContentDetectionRequest`,
  // `NotificationContentDetectionResponse`, and
  // `NotificationContentDetectionQuality`, for logging.
  auto ncd_request = std::make_unique<
      optimization_guide::proto::NotificationContentDetectionRequest>();
  *ncd_request->mutable_notification_contents() = *notification_contents;
  auto ncd_response = std::make_unique<
      optimization_guide::proto::NotificationContentDetectionResponse>();
  auto ncd_quality = std::make_unique<
      optimization_guide::proto::NotificationContentDetectionQuality>();

  // Add metadata to log if it's defined in the `notification_database_data`.
  if (notification_database_data.serialized_metadata.contains(
          safe_browsing::kMetadataDictionaryKey)) {
    JSONStringValueDeserializer deserializer(
        notification_database_data.serialized_metadata.at(
            safe_browsing::kMetadataDictionaryKey));
    std::unique_ptr<base::Value> metadata =
        deserializer.Deserialize(nullptr, nullptr);
    if (!metadata.get() || !metadata->is_dict()) {
      DVLOG(1) << "Failed to parse metadata.";
    } else {
      auto metadata_dict = std::move(*metadata).TakeDict();
      if (metadata_dict.FindBool(kMetadataIsOriginAllowlistedByUserKey)
              .has_value()) {
        ncd_quality->set_did_user_always_allow_url(
            metadata_dict.FindBool(kMetadataIsOriginAllowlistedByUserKey)
                .value());
      }
      if (metadata_dict.FindBool(kMetadataIsOriginOnGlobalCacheListKey)
              .has_value()) {
        ncd_quality->set_is_url_on_allowlist(
            metadata_dict.FindBool(kMetadataIsOriginOnGlobalCacheListKey)
                .value());
      }
      if (metadata_dict.FindDouble(kMetadataSuspiciousKey).has_value()) {
        ncd_response->set_suspicious_score(
            metadata_dict.FindDouble(kMetadataSuspiciousKey).value());
      }
    }
  }
  ncd_quality->set_was_user_shown_warning(mqls_metadata.did_show_warning_);
  ncd_quality->set_did_user_unsubscribe(mqls_metadata.did_user_unsubscribe_);
  ncd_quality->set_site_engagement_score(
      EngagementLevelToProtoScore(mqls_metadata.site_engagement_score_));

  // Create `NotificationContentDetectionLoggingData`, for uploading the log.
  std::unique_ptr<
      optimization_guide::proto::NotificationContentDetectionLoggingData>
      logging_data = std::make_unique<
          optimization_guide::proto::NotificationContentDetectionLoggingData>();
  *logging_data->mutable_request() = *ncd_request;
  *logging_data->mutable_response() = *ncd_response;
  *logging_data->mutable_quality() = *ncd_quality;

  // Upload log.
  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_service);
  *log_entry->log_ai_data_request()->mutable_notification_content_detection() =
      *logging_data;
  optimization_guide::ModelQualityLogEntry::Upload(std::move(log_entry));
}

}  // namespace safe_browsing
