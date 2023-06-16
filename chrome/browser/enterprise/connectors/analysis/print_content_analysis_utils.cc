// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/print_content_analysis_utils.h"

#include <cstring>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/web_contents.h"
#include "printing/printing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

void ScanAndPrint(scoped_refptr<base::RefCountedMemory> data,
                  content::WebContents* initiator,
                  ContentAnalysisDelegate::Data scanning_data,
                  base::OnceCallback<void(bool)> on_verdict) {
  // The preview document bytes are copied so that the content analysis code
  // can arbitrarily use them without having to handle ownership issues with
  // other printing code.
  base::MappedReadOnlyRegion region =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!region.IsValid()) {
    // Allow printing if the scan can't happen due to memory failure.
    PRINTER_LOG(ERROR) << "Printed without analysis due to memory failure";
    std::move(on_verdict).Run(/*allowed=*/true);
    return;
  }
  std::memcpy(region.mapping.memory(), data->front(), data->size());
  scanning_data.page = std::move(region.region);

  auto on_scan_result = base::BindOnce(
      [](base::OnceCallback<void(bool should_proceed)> callback,
         const ContentAnalysisDelegate::Data& data,
         ContentAnalysisDelegate::Result& result) {
        std::move(callback).Run(result.page_result);
      },
      std::move(on_verdict));
  ContentAnalysisDelegate::CreateForWebContents(
      initiator, std::move(scanning_data), std::move(on_scan_result),
      safe_browsing::DeepScanAccessPoint::PRINT);
}

}  // namespace

void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> data,
                            content::WebContents* initiator,
                            std::string printer_name,
                            base::OnceCallback<void(bool)> on_verdict,
                            base::OnceClosure hide_preview) {
  ContentAnalysisDelegate::Data scanning_data;

  if (ContentAnalysisDelegate::IsEnabled(
          Profile::FromBrowserContext(initiator->GetBrowserContext()),
          initiator->GetLastCommittedURL(), &scanning_data,
          AnalysisConnector::PRINT) &&
      base::FeatureList::IsEnabled(
          printing::features::kEnableLocalScanAfterPreview) &&
      scanning_data.settings.cloud_or_local_settings.is_local_analysis()) {
    // Populate print metadata.
    scanning_data.printer_name = std::move(printer_name);

    // Hide the preview dialog so it doesn't cover the content analysis dialog
    // showing the status of the scanning.
    // TODO(b/281087582): May need to be handled differently when the scan
    // takes place in the cloud instead of locally.
    std::move(hide_preview).Run();
    ScanAndPrint(data, initiator, std::move(scanning_data),
                 std::move(on_verdict));
    return;
  }
  std::move(on_verdict).Run(/*allowed=*/true);
}

absl::optional<ContentAnalysisDelegate::Data> GetBeforePrintPreviewAnalysisData(
    content::WebContents* web_contents) {
  ContentAnalysisDelegate::Data scanning_data;

  if (!ContentAnalysisDelegate::IsEnabled(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          web_contents->GetOutermostWebContents()->GetLastCommittedURL(),
          &scanning_data, AnalysisConnector::PRINT)) {
    return absl::nullopt;
  }

  if (base::FeatureList::IsEnabled(
          printing::features::kEnableLocalScanAfterPreview) &&
      scanning_data.settings.cloud_or_local_settings.is_local_analysis()) {
    return absl::nullopt;
  }

  return scanning_data;
}

}  // namespace enterprise_connectors
