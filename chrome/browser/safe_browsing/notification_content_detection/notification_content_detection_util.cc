// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_util.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
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

// Extracts the notification content detection metadata dictionary from the
// notification database data. Returns std::nullopt if the metadata is not
// present or cannot be parsed.
std::optional<base::Value::Dict> GetNotificationContentMetadata(
    const content::NotificationDatabaseData& notification_database_data) {
  const auto& metadata_it = notification_database_data.serialized_metadata.find(
      safe_browsing::kNotificationContentDetectionMetadataDictionaryKey);
  if (metadata_it == notification_database_data.serialized_metadata.end()) {
    return std::nullopt;
  }

  JSONStringValueDeserializer deserializer(metadata_it->second);
  std::unique_ptr<base::Value> metadata_value =
      deserializer.Deserialize(nullptr, nullptr);

  if (!metadata_value || !metadata_value->is_dict()) {
    DVLOG(1) << "Failed to parse notification content metadata.";
    return std::nullopt;
  }

  return std::move(*metadata_value).TakeDict();
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
  std::optional<base::Value::Dict> metadata_dict =
      GetNotificationContentMetadata(notification_database_data);
  if (metadata_dict.has_value()) {
    std::optional<bool> is_origin_allowlisted_by_user =
        metadata_dict->FindBool(kMetadataIsOriginAllowlistedByUserKey);
    if (is_origin_allowlisted_by_user.has_value()) {
      ncd_quality->set_did_user_always_allow_url(
          is_origin_allowlisted_by_user.value());
    }
    std::optional<bool> is_origin_on_global_cache_list =
        metadata_dict->FindBool(kMetadataIsOriginOnGlobalCacheListKey);
    if (is_origin_on_global_cache_list.has_value()) {
      ncd_quality->set_is_url_on_allowlist(
          is_origin_on_global_cache_list.value());
    }
    std::optional<double> suspicious_score =
        metadata_dict->FindDouble(kMetadataSuspiciousScoreKey);
    if (suspicious_score.has_value()) {
      ncd_response->set_suspicious_score(suspicious_score.value());
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

void NotificationContentDetectionUkmUtil::
    RecordSuspiciousNotificationInteractionUkm(int suspicious_interaction_type,
                                               const GURL& requesting_origin,
                                               std::string notification_id,
                                               Profile* profile) {
  if (profile && profile->GetStoragePartitionForUrl(requesting_origin) &&
      profile->GetStoragePartitionForUrl(requesting_origin)
          ->GetPlatformNotificationContext() &&
      !notification_id.empty()) {
    auto* notification_context =
        profile->GetStoragePartitionForUrl(requesting_origin)
            ->GetPlatformNotificationContext();
    notification_context->ReadNotificationDataAndRecordInteraction(
        notification_id, requesting_origin,
        content::PlatformNotificationContext::Interaction::NONE,
        base::BindOnce(&NotificationContentDetectionUkmUtil::
                           DoRecordSuspiciousNotificationInteractionUkm,
                       suspicious_interaction_type, requesting_origin));
  } else {
    DoRecordSuspiciousNotificationInteractionUkm(
        suspicious_interaction_type, requesting_origin,
        /*is_database_data_found=*/false, content::NotificationDatabaseData());
  }
}

void NotificationContentDetectionUkmUtil::
    DoRecordSuspiciousNotificationInteractionUkm(
        int suspicious_interaction_type,
        const GURL& requesting_origin,
        bool is_database_data_found,
        const content::NotificationDatabaseData& notification_database_data) {
  // Setup builder for logging the UKM.
  ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForNotificationEvent(
      base::PassKey<NotificationContentDetectionUkmUtil>(), requesting_origin);
  ukm::builders::SuspiciousNotificationInteraction builder(source_id);
  builder.SetSuspiciousInteractionType(suspicious_interaction_type);

  // If a suspicious score can be found in `notification_database_data`, then
  // set the value on the UKM builder.
  if (is_database_data_found) {
    std::optional<base::Value::Dict> metadata_dict =
        GetNotificationContentMetadata(notification_database_data);
    if (metadata_dict.has_value()) {
      std::optional<double> suspicious_score =
          metadata_dict->FindDouble(kMetadataSuspiciousScoreKey);
      if (suspicious_score.has_value()) {
        builder.SetSuspiciousScore(suspicious_score.value());
      }
    }
  }

  // Log the UKM.
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace safe_browsing
