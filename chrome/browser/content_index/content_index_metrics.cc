// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

void MaybeRecordUkmContentAdded(blink::mojom::ContentCategory category,
                                std::optional<ukm::SourceId> source_id) {
  if (!source_id)
    return;

  ukm::builders::ContentIndex_Added(*source_id)
      .SetCategory(static_cast<int>(category))
      .Record(ukm::UkmRecorder::Get());
}

void MaybeRecordUkmContentDeletedByUser(
    std::optional<ukm::SourceId> source_id) {
  if (!source_id)
    return;

  ukm::builders::ContentIndex_DeletedByUser(*source_id)
      .SetDeleted(true)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

ContentIndexMetrics::ContentIndexMetrics(
    ukm::UkmBackgroundRecorderService* ukm_background_service)
    : ukm_background_service_(ukm_background_service) {
  DCHECK(ukm_background_service_);
}

ContentIndexMetrics::~ContentIndexMetrics() = default;

void ContentIndexMetrics::RecordContentAdded(
    const url::Origin& origin,
    blink::mojom::ContentCategory category) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(&MaybeRecordUkmContentAdded, category));
}

void ContentIndexMetrics::RecordContentOpened(
    content::WebContents* web_contents,
    blink::mojom::ContentCategory category) {
  ukm::builders::ContentIndex_Opened(
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetIsOffline(net::NetworkChangeNotifier::IsOffline())
      .Record(ukm::UkmRecorder::Get());
}

void ContentIndexMetrics::RecordContentDeletedByUser(
    const url::Origin& origin) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(&MaybeRecordUkmContentDeletedByUser));
}
