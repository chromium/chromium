// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include <algorithm>
#include <memory>
#include <queue>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"
#include "chrome/browser/enterprise/data_controls/reporting_service.h"
#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/content/clipboard_restriction_service.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/data_controls_dialog_factory.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_factory.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

namespace enterprise_data_protection {

namespace {

// Returns an empty URL if `endpoint` doesn't hold a DTE, or a non-URL DTE.
GURL GetUrlFromEndpoint(const content::ClipboardEndpoint& endpoint) {
  if (!endpoint.data_transfer_endpoint() ||
      !endpoint.data_transfer_endpoint()->IsUrlType() ||
      !endpoint.data_transfer_endpoint()->GetURL()) {
    return GURL();
  }
  return *endpoint.data_transfer_endpoint()->GetURL();
}

bool SkipDataControlOrContentAnalysisChecks(
    const content::ClipboardEndpoint& main_endpoint) {
  // Data Controls and content analysis copy/paste checks require an active tab
  // to be meaningful, so if it's gone they can be skipped.
  auto* web_contents = main_endpoint.web_contents();
  if (!web_contents) {
    return true;
  }

  return false;
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void HandleFileData(
    content::WebContents* web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  enterprise_connectors::ContentAnalysisDelegate::CreateForFilesInWebContents(
      web_contents, std::move(dialog_data),
      base::BindOnce(
          [](content::ContentBrowserClient::IsClipboardPasteAllowedCallback
                 callback,
             std::vector<base::FilePath> paths, std::vector<bool> results) {
            std::optional<content::ClipboardPasteData> clipboard_paste_data;
            bool all_blocked =
                std::all_of(results.begin(), results.end(),
                            [](bool allowed) { return !allowed; });
            if (!all_blocked) {
              std::vector<base::FilePath> allowed_paths;
              allowed_paths.reserve(paths.size());
              for (size_t i = 0; i < paths.size(); ++i) {
                if (results[i]) {
                  allowed_paths.emplace_back(std::move(paths[i]));
                }
              }
              clipboard_paste_data = content::ClipboardPasteData();
              clipboard_paste_data->file_paths = std::move(allowed_paths);
            }
            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PASTE);
}

void HandleStringData(
    content::WebContents* web_contents,
    content::ClipboardPasteData clipboard_paste_data,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents, std::move(dialog_data),
      base::BindOnce(
          [](content::ClipboardPasteData clipboard_paste_data,
             content::ContentBrowserClient::IsClipboardPasteAllowedCallback
                 callback,
             const enterprise_connectors::ContentAnalysisDelegate::Data& data,
             enterprise_connectors::ContentAnalysisDelegate::Result& result) {
            // TODO(b/318664590): Since the `data` argument is forwarded to
            // `callback`, changing the type from `const Data&` to just `Data`
            // would avoid a copy.

            bool text_blocked =
                result.text_results.empty() || !result.text_results[0];
            if (text_blocked && !result.image_result) {
              std::move(callback).Run(std::nullopt);
              return;
            }

            if (text_blocked) {
              clipboard_paste_data.text.clear();
              clipboard_paste_data.html.clear();
              clipboard_paste_data.svg.clear();
              clipboard_paste_data.rtf.clear();
              clipboard_paste_data.custom_data.clear();
            }
            if (!result.image_result) {
              clipboard_paste_data.png.clear();
            }

            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(clipboard_paste_data), std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PASTE);
}

void PasteIfAllowedByContentAnalysis(
    content::WebContents* web_contents,
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  DCHECK(web_contents);
  DCHECK(!SkipDataControlOrContentAnalysisChecks(destination));

  // Always allow if the source of the last clipboard commit was this host.
  if (destination.web_contents()->GetPrimaryMainFrame()->IsClipboardOwner(
          metadata.seqno)) {
    ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno,
                                                  clipboard_paste_data);
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  Profile* profile = Profile::FromBrowserContext(destination.browser_context());
  if (!profile) {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  bool is_files =
      metadata.format_type == ui::ClipboardFormatType::FilenamesType();
  enterprise_connectors::AnalysisConnector connector =
      is_files ? enterprise_connectors::AnalysisConnector::FILE_ATTACHED
               : enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY;
  enterprise_connectors::ContentAnalysisDelegate::Data dialog_data;

  if (!enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          profile, GetUrlFromEndpoint(destination), &dialog_data, connector)) {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  dialog_data.reason =
      enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE;
  dialog_data.clipboard_source =
      data_controls::ReportingService::GetClipboardSourceString(
          source, destination,
          enterprise_connectors::kOnBulkDataEntryScopePref);

  if (is_files) {
    dialog_data.paths = std::move(clipboard_paste_data.file_paths);
    HandleFileData(web_contents, std::move(dialog_data), std::move(callback));
  } else {
    dialog_data.AddClipboardData(clipboard_paste_data);
    HandleStringData(web_contents, std::move(clipboard_paste_data),
                     std::move(dialog_data), std::move(callback));
  }
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

data_controls::DataControlsDialogFactory* GetDialogFactory() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  return data_controls::DesktopDataControlsDialogFactory::GetInstance();
#endif
}

void MaybeReportDataControlsPaste(const content::ClipboardEndpoint& source,
                                  const content::ClipboardEndpoint& destination,
                                  const content::ClipboardMetadata& metadata,
                                  const data_controls::Verdict& verdict,
                                  bool bypassed = false) {
#if !BUILDFLAG(IS_ANDROID)
  auto* reporting_service =
      data_controls::ReportingServiceFactory::GetInstance()
          ->GetForBrowserContext(destination.browser_context());

  // `reporting_service` can be null for incognito browser contexts, so since
  // there's no reporting in that case we just return early.
  if (!reporting_service) {
    return;
  }

  if (bypassed) {
    reporting_service->ReportPasteWarningBypassed(source, destination, metadata,
                                                  verdict);
  } else {
    reporting_service->ReportPaste(source, destination, metadata, verdict);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void MaybeReportDataControlsCopy(const content::ClipboardEndpoint& source,
                                 const content::ClipboardMetadata& metadata,
                                 const data_controls::Verdict& verdict,
                                 bool bypassed = false) {
#if !BUILDFLAG(IS_ANDROID)
  auto* reporting_service =
      data_controls::ReportingServiceFactory::GetInstance()
          ->GetForBrowserContext(source.browser_context());

  // `reporting_service` can be null for incognito browser contexts, so since
  // there's no reporting in that case we just return early.
  if (!reporting_service) {
    return;
  }

  if (bypassed) {
    reporting_service->ReportCopyWarningBypassed(source, metadata, verdict);
  } else {
    reporting_service->ReportCopy(source, metadata, verdict);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void OnDataControlsPasteWarning(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    data_controls::Verdict verdict,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback,
    bool bypassed) {
  if (!bypassed || SkipDataControlOrContentAnalysisChecks(destination)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (bypassed && verdict.level() == data_controls::Rule::Level::kWarn) {
    MaybeReportDataControlsPaste(source, destination, metadata, verdict,
                                 /*bypassed=*/true);
  }

#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::move(clipboard_paste_data));
#else
  PasteIfAllowedByContentAnalysis(
      destination.web_contents(), source, destination, metadata,
      std::move(clipboard_paste_data), std::move(callback));
#endif  // BUILDFLAG(IS_ANDROID)
}

void PasteIfAllowedByDataControls(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  DCHECK(!SkipDataControlOrContentAnalysisChecks(destination));

  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(destination.browser_context())
                     ->GetPasteVerdict(source, destination, metadata);
  if (source.browser_context() &&
      source.browser_context() != destination.browser_context()) {
    verdict = data_controls::Verdict::MergePasteVerdicts(
        data_controls::ChromeRulesServiceFactory::GetInstance()
            ->GetForBrowserContext(source.browser_context())
            ->GetPasteVerdict(source, destination, metadata),
        std::move(verdict));
  }

  auto* factory = GetDialogFactory();
  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    MaybeReportDataControlsPaste(source, destination, metadata, verdict);
    if (factory) {
      factory->ShowDialogIfNeeded(
          destination.web_contents(),
          data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    }
    std::move(callback).Run(std::nullopt);
    return;
  } else if (verdict.level() == data_controls::Rule::Level::kWarn) {
    MaybeReportDataControlsPaste(source, destination, metadata, verdict);
    if (factory) {
      factory->ShowDialogIfNeeded(
          destination.web_contents(),
          data_controls::DataControlsDialog::Type::kClipboardPasteWarn,
          base::BindOnce(&OnDataControlsPasteWarning, source, destination,
                         metadata, std::move(verdict),
                         std::move(clipboard_paste_data), std::move(callback)));
    } else {
      std::move(callback).Run(std::nullopt);
    }
    return;
  } else if (verdict.level() == data_controls::Rule::Level::kReport) {
    MaybeReportDataControlsPaste(source, destination, metadata, verdict);
  }

  // If the data currently being pasted was replaced when it was initially
  // copied from Chrome, replace it back since it hasn't triggered a Data
  // Controls rule when pasting. Only do this if `source` has a known browser
  // context to ensure we're not letting through data that was replaced by
  // policies that are no longer applicable due to the profile being closed.
  if (source.browser_context() &&
      metadata.seqno == data_controls::GetLastReplacedClipboardData().seqno) {
    clipboard_paste_data =
        data_controls::GetLastReplacedClipboardData().clipboard_paste_data;
  }

#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::move(clipboard_paste_data));
#else
  PasteIfAllowedByContentAnalysis(
      destination.web_contents(), source, destination, metadata,
      std::move(clipboard_paste_data), std::move(callback));
#endif  // BUILDFLAG(IS_ANDROID)
}

void OnDlpRulesCheckDone(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback,
    bool allowed) {
  // If DLP rules blocked the action or if there are no further policy checks
  // required, return null to indicate the pasting is blocked or no longer
  // applicable.
  if (!allowed || SkipDataControlOrContentAnalysisChecks(destination)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  PasteIfAllowedByDataControls(source, destination, metadata,
                               std::move(clipboard_paste_data),
                               std::move(callback));
}

void IsCopyToOSClipboardRestricted(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(source.browser_context())
                     ->GetCopyToOSClipboardVerdict(GetUrlFromEndpoint(source));

  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    // Before calling `callback`, we remember `data` will correspond to the next
    // clipboard sequence number so that it can be potentially replaced again at
    // paste time.
    data_controls::LastReplacedClipboardDataObserver::GetInstance()
        ->AddDataToNextSeqno(data);
    std::move(callback).Run(
        metadata.format_type, data, /*replacement_data=*/
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE));

    return;
  }

  std::move(callback).Run(metadata.format_type, data, std::nullopt);
}

void OnDataControlsCopyWarning(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    data_controls::Verdict verdict,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback,
    bool bypassed) {
  if (bypassed) {
    MaybeReportDataControlsCopy(source, metadata, verdict, /*bypassed=*/true);
    IsCopyToOSClipboardRestricted(source, metadata, data, std::move(callback));
    return;
  }
}

void IsCopyRestrictedByDialog(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  auto source_only_verdict =
      data_controls::ChromeRulesServiceFactory::GetInstance()
          ->GetForBrowserContext(source.browser_context())
          ->GetCopyRestrictedBySourceVerdict(GetUrlFromEndpoint(source));

  auto* factory = GetDialogFactory();
  if (source_only_verdict.level() == data_controls::Rule::Level::kBlock) {
    MaybeReportDataControlsCopy(source, metadata, source_only_verdict);
    if (factory) {
      factory->ShowDialogIfNeeded(
          source.web_contents(),
          data_controls::DataControlsDialog::Type::kClipboardCopyBlock);
    }
    return;
  }

  // The "warn" level of copying to the OS clipboard is to show a warning
  // dialog, not to do a string replacement.
  auto os_clipboard_verdict =
      data_controls::ChromeRulesServiceFactory::GetInstance()
          ->GetForBrowserContext(source.browser_context())
          ->GetCopyToOSClipboardVerdict(GetUrlFromEndpoint(source));

  if (source_only_verdict.level() == data_controls::Rule::Level::kWarn ||
      os_clipboard_verdict.level() == data_controls::Rule::Level::kWarn) {
    auto verdict = data_controls::Verdict::MergeCopyWarningVerdicts(
        std::move(source_only_verdict), std::move(os_clipboard_verdict));
    MaybeReportDataControlsCopy(source, metadata, verdict);
    if (factory) {
      factory->ShowDialogIfNeeded(
          source.web_contents(),
          data_controls::DataControlsDialog::Type::kClipboardCopyWarn,
          base::BindOnce(&OnDataControlsCopyWarning, source, metadata, data,
                         std::move(verdict), std::move(callback)));
    }
    return;
  }

  if (source_only_verdict.level() == data_controls::Rule::Level::kReport) {
    MaybeReportDataControlsCopy(source, metadata, source_only_verdict);
  }

  IsCopyToOSClipboardRestricted(source, metadata, data, std::move(callback));
}

}  // namespace

void PasteIfAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  if (ui::DataTransferPolicyController::HasInstance()) {
    absl::variant<size_t, std::vector<base::FilePath>> pasted_content;
    if (clipboard_paste_data.file_paths.empty()) {
      DCHECK(metadata.size.has_value());
      pasted_content = *metadata.size;
    } else {
      pasted_content = clipboard_paste_data.file_paths;
    }

    std::optional<ui::DataTransferEndpoint> destination_endpoint = std::nullopt;
    if (destination.browser_context() &&
        !destination.browser_context()->IsOffTheRecord()) {
      destination_endpoint = destination.data_transfer_endpoint();
    }

    ui::DataTransferPolicyController::Get()->PasteIfAllowed(
        source.data_transfer_endpoint(), destination_endpoint,
        std::move(pasted_content),
        destination.web_contents()
            ? destination.web_contents()->GetPrimaryMainFrame()
            : nullptr,
        base::BindOnce(&OnDlpRulesCheckDone, source, destination, metadata,
                       std::move(clipboard_paste_data), std::move(callback)));
    return;
  }

  OnDlpRulesCheckDone(source, destination, metadata,
                      std::move(clipboard_paste_data), std::move(callback),
                      /*allowed=*/true);
}

void IsClipboardCopyAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  DCHECK(source.web_contents());
  DCHECK(source.browser_context());

#if !BUILDFLAG(IS_ANDROID)
  std::u16string replacement_data;
  ClipboardRestrictionService* service =
      ClipboardRestrictionServiceFactory::GetInstance()->GetForBrowserContext(
          source.browser_context());
  if (!service->IsUrlAllowedToCopy(GetUrlFromEndpoint(source),
                                   metadata.size.value_or(0),
                                   &replacement_data)) {
    std::move(callback).Run(metadata.format_type, data,
                            std::move(replacement_data));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  IsCopyRestrictedByDialog(source, metadata, data, std::move(callback));
}

void ReplaceSameTabClipboardDataIfRequiredByPolicy(
    ui::ClipboardSequenceNumberToken seqno,
    content::ClipboardPasteData& data) {
  if (seqno == data_controls::GetLastReplacedClipboardData().seqno) {
    data = data_controls::GetLastReplacedClipboardData().clipboard_paste_data;
  }
}

}  // namespace enterprise_data_protection
