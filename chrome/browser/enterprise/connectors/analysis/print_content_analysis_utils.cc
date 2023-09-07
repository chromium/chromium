// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/print_content_analysis_utils.h"

#include <cstring>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_event_log/device_event_log.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "printing/printing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

bool ShouldDoScan(bool post_dialog_feature_enabled,
                  PrintScanningContext context) {
  if (post_dialog_feature_enabled) {
    switch (context) {
      // For "normal" prints, the scanning can happen immediately after the user
      // clicks "Print" in the print preview dialog as the preview document is
      // representative of what they are printing.
      case PrintScanningContext::kNormalPrintAfterPreview:
        return true;
      case PrintScanningContext::kBeforePreview:
      case PrintScanningContext::kNormalPrintBeforePrintDocument:
        return false;

      // For "system dialog" prints, the scanning waits until the user picks
      // settings from the system dialog, but starts applying enterprise-logic
      // logic at the `kBeforeSystemDialog` context for that to happen.
      //
      // Scanning also happens right before the document is printed through an
      // existing print job when that is triggered after the print preview
      // dialog.
      case PrintScanningContext::kBeforeSystemDialog:
        return true;
      case PrintScanningContext::kSystemPrintAfterPreview:
        return false;
      case PrintScanningContext::kSystemPrintBeforePrintDocument:
        return true;

#if BUILDFLAG(IS_MAC)
      // For the "Open PDF in Preview" option on Mac, scan right as it happens
      // from the print preview dialog.
      case PrintScanningContext::kOpenPdfInPreview:
        return true;
#endif  // BUILDFLAG(IS_MAC)
    }
  }

  // `post_dialog_feature_enabled` being false means printing should only happen
  // before any kind of dialog to get settings is shown.
  switch (context) {
    case PrintScanningContext::kBeforePreview:
    case PrintScanningContext::kBeforeSystemDialog:
      return true;

    case PrintScanningContext::kNormalPrintAfterPreview:
    case PrintScanningContext::kSystemPrintAfterPreview:
    case PrintScanningContext::kNormalPrintBeforePrintDocument:
    case PrintScanningContext::kSystemPrintBeforePrintDocument:
#if BUILDFLAG(IS_MAC)
    case PrintScanningContext::kOpenPdfInPreview:
#endif  // BUILDFLAG(IS_MAC)
      return false;
  }
}

bool ShouldScan(PrintScanningContext context,
                const ContentAnalysisDelegate::Data& scanning_data) {
  return scanning_data.settings.cloud_or_local_settings.is_local_analysis()
             ? ShouldDoScan(
                   base::FeatureList::IsEnabled(
                       printing::features::kEnableLocalScanAfterPreview),
                   context)
             : ShouldDoScan(
                   base::FeatureList::IsEnabled(
                       printing::features::kEnableCloudScanAfterPreview),
                   context);
}

void RecordPrintType(PrintScanningContext context,
                     const ContentAnalysisDelegate::Data& scanning_data) {
  if (scanning_data.settings.cloud_or_local_settings.is_local_analysis()) {
    base::UmaHistogramEnumeration("Enterprise.OnPrint.Local.PrintType",
                                  context);
  } else {
    base::UmaHistogramEnumeration("Enterprise.OnPrint.Cloud.PrintType",
                                  context);
  }
}

}  // namespace

void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> print_data,
                            content::WebContents* initiator,
                            std::string printer_name,
                            PrintScanningContext context,
                            base::OnceCallback<void(bool)> on_verdict,
                            base::OnceClosure hide_preview) {
  DCHECK(initiator);

  // This needs to be done to avoid having an embedded page compare against
  // policies and report its URL. This is especially important when Chrome's PDF
  // reader is used, as a cryptic extension URL will be sent for
  // scanning/reporting in that case.
  // TODO(b/289243948): Add browser test coverage for web contents received and
  // passed to the delegate.
  content::WebContents* web_contents = initiator->GetOutermostWebContents();

  absl::optional<ContentAnalysisDelegate::Data> scanning_data =
      GetPrintAnalysisData(web_contents, context);

  if (!scanning_data) {
    std::move(on_verdict).Run(/*allowed=*/true);
    return;
  }

  // Populate print metadata.
  scanning_data->printer_name = std::move(printer_name);

  // Hide the preview dialog so it doesn't cover the content analysis dialog
  // showing the status of the scanning. Since that scanning dialog only appears
  // when the policy is set to be blocking, don't run `hide_preview` for other
  // policy configurations to avoid races between the dialog being hidden vs
  // destroyed.
  if (scanning_data->settings.block_until_verdict ==
      BlockUntilVerdict::kBlock) {
    std::move(hide_preview).Run();
  }

  PrintIfAllowedByPolicy(print_data, web_contents, std::move(*scanning_data),
                         std::move(on_verdict));
}

void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> print_data,
                            content::WebContents* initiator,
                            ContentAnalysisDelegate::Data scanning_data,
                            base::OnceCallback<void(bool)> on_verdict) {
  // The preview document bytes are copied so that the content analysis code
  // can arbitrarily use them without having to handle ownership issues with
  // other printing code.
  base::MappedReadOnlyRegion region =
      base::ReadOnlySharedMemoryRegion::Create(print_data->size());
  if (!region.IsValid()) {
    // Allow printing if the scan can't happen due to memory failure.
    PRINTER_LOG(ERROR) << "Printed without analysis due to memory failure";
    std::move(on_verdict).Run(/*allowed=*/true);
    return;
  }
  std::memcpy(region.mapping.memory(), print_data->front(), print_data->size());
  scanning_data.page = std::move(region.region);

  auto on_scan_result = base::BindOnce(
      [](base::OnceCallback<void(bool should_proceed)> callback,
         const ContentAnalysisDelegate::Data& data,
         ContentAnalysisDelegate::Result& result) {
        std::move(callback).Run(result.page_result);
      },
      std::move(on_verdict));

  // This needs to be done to avoid having an embedded page compare against
  // policies and report its URL. This is especially important when Chrome's PDF
  // reader is used, as a cryptic extension URL will be sent for
  // scanning/reporting in that case.
  // TODO(b/289243948): Add browser test coverage for web contents received and
  // passed to the delegate.
  content::WebContents* web_contents = initiator->GetOutermostWebContents();

  ContentAnalysisDelegate::CreateForWebContents(
      web_contents, std::move(scanning_data), std::move(on_scan_result),
      safe_browsing::DeepScanAccessPoint::PRINT);
}

absl::optional<ContentAnalysisDelegate::Data> GetPrintAnalysisData(
    content::WebContents* web_contents,
    PrintScanningContext context) {
  ContentAnalysisDelegate::Data scanning_data;

  bool enabled = ContentAnalysisDelegate::IsEnabled(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      web_contents->GetOutermostWebContents()->GetLastCommittedURL(),
      &scanning_data, AnalysisConnector::PRINT);

  if (enabled && ShouldScan(context, scanning_data)) {
    // Returning a non-null value here means the user triggered an action
    // leading to a scan, so logging the print type metric here will apply it to
    // every print content analysis workflow.
    RecordPrintType(context, scanning_data);

    switch (context) {
#if BUILDFLAG(IS_MAC)
      case PrintScanningContext::kOpenPdfInPreview:
#endif  // BUILDFLAG(IS_MAC)
      case PrintScanningContext::kNormalPrintAfterPreview:
      case PrintScanningContext::kBeforePreview:
      case PrintScanningContext::kNormalPrintBeforePrintDocument:
        scanning_data.reason = ContentAnalysisRequest::PRINT_PREVIEW_PRINT;
        break;

      case PrintScanningContext::kBeforeSystemDialog:
      case PrintScanningContext::kSystemPrintAfterPreview:
      case PrintScanningContext::kSystemPrintBeforePrintDocument:
        scanning_data.reason = ContentAnalysisRequest::SYSTEM_DIALOG_PRINT;
        break;
    }

    return scanning_data;
  }

  return absl::nullopt;
}

}  // namespace enterprise_connectors
