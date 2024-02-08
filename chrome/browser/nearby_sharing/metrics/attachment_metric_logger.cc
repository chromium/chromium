// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/attachment_metric_logger.h"

#include <utility>

#include "chrome/browser/nearby_sharing/metrics/metric_common.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace nearby::share::metrics {

AttachmentMetricLogger::AttachmentMetricLogger() = default;
AttachmentMetricLogger::~AttachmentMetricLogger() {}

// TODO(b/266739400): Test this once there is Structured Metrics unittesting
// infrastructure available.
void AttachmentMetricLogger::OnTransferCompleted(
    const ShareTarget& share_target,
    TransferMetadata::Status status) {
  auto platform = GetPlatform(share_target);
  auto relationship = GetDeviceRelationship(share_target);
  auto result = GetTransferResult(status);

  // WiFi credential attachment metadata is not logged here, but their count is
  // captured in the overall share metrics logged elsewhere.

  for (auto attachment : share_target.file_attachments) {
    ::metrics::structured::StructuredMetricsClient::Record(std::move(
        ::metrics::structured::events::v2::nearby_share::FileAttachment()
            .SetIsReceiving(share_target.is_incoming)
            .SetPlatform(static_cast<int>(platform))
            .SetDeviceRelationship(static_cast<int>(relationship))
            .SetFileType(static_cast<int>(attachment.type()))
            .SetResult(static_cast<int>(result))
            .SetSize(attachment.size())));
  }

  for (auto attachment : share_target.text_attachments) {
    ::metrics::structured::StructuredMetricsClient::Record(std::move(
        ::metrics::structured::events::v2::nearby_share::TextAttachment()
            .SetIsReceiving(share_target.is_incoming)
            .SetPlatform(static_cast<int>(platform))
            .SetDeviceRelationship(static_cast<int>(relationship))
            .SetTextType(static_cast<int>(attachment.type()))
            .SetResult(static_cast<int>(result))
            .SetSize(attachment.size())));
  }
}

}  // namespace nearby::share::metrics
