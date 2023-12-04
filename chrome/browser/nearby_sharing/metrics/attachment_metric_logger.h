// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_METRICS_ATTACHMENT_METRIC_LOGGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_METRICS_ATTACHMENT_METRIC_LOGGER_H_

#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

namespace nearby::share::metrics {

class AttachmentMetricLogger : public NearbySharingService::Observer {
 public:
  AttachmentMetricLogger();
  ~AttachmentMetricLogger() override;

  // NearbySharingService::Observer
  void OnTransferCompleted(const ShareTarget& share_target,
                           TransferMetadata::Status status) override;
};

}  // namespace nearby::share::metrics

#endif  // CHROME_BROWSER_NEARBY_SHARING_METRICS_ATTACHMENT_METRIC_LOGGER_H_
