// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <variant>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_util.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/data_controls/chrome_clipboard_context.h"
#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog_factory.h"
#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/content/clipboard_restriction_service.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_factory.h"
#endif  // BUILDFLAG(ENTERPRISE_DATA_PROTECTION)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

namespace enterprise_data_protection {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataControlsDragAndDropVerdict {
  kAllowed = 0,
  kBlocked = 1,
  kWarned = 2,
  kMaxValue = kWarned
};

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
      enterprise_connectors::DeepScanAccessPoint::PASTE);
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
                !result.text_results.empty() && !result.text_results[0];

            // Image scan results are ignore for non local scans, unless the
            // kDlpScanPastedImages feature is enabled.
            bool image_blocked = false;
            if (data.settings.cloud_or_local_settings.is_local_analysis() ||
                base::FeatureList::IsEnabled(
                    enterprise_connectors::kDlpScanPastedImages)) {
              image_blocked =
                  !clipboard_paste_data.png.empty() && !result.image_result;
            }

            if (text_blocked || image_blocked) {
              std::move(callback).Run(std::nullopt);
              return;
            }

            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(clipboard_paste_data), std::move(callback)),
      enterprise_connectors::DeepScanAccessPoint::PASTE);
}

void PasteIfAllowedByContentAnalysis(
    content::WebContents* web_contents,
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
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
      data_controls::ChromeClipboardContext::GetClipboardSource(
          source, destination,
          enterprise_connectors::kOnBulkDataEntryScopePref);
  dialog_data.source_content_area_email =
      enterprise_connectors::ContentAreaUserProvider::GetUser(source);

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
  return data_controls::AndroidDataControlsDialogFactory::GetInstance();
#elif BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  return data_controls::DesktopDataControlsDialogFactory::GetInstance();
#else
  return nullptr;
#endif
}

void MaybeReportDataControlsPaste(const content::ClipboardEndpoint& source,
                                  const content::ClipboardEndpoint& destination,
                                  const ui::ClipboardMetadata& metadata,
                                  const data_controls::Verdict& verdict,
                                  bool bypassed = false) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          destination.browser_context());

  // `router` can be null for incognito browser contexts, so since there's no
  // reporting in that case we just return early.
  if (!router) {
    return;
  }

  data_controls::ChromeClipboardContext context(source, destination, metadata);

  if (bypassed) {
    router->ReportPasteWarningBypassed(context, verdict);
  } else {
    router->ReportPaste(context, verdict);
  }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

void MaybeReportDataControlsCopy(const content::ClipboardEndpoint& source,
                                 const ui::ClipboardMetadata& metadata,
                                 const data_controls::Verdict& verdict,
                                 bool bypassed = false) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          source.browser_context());

  // `router` can be null for incognito browser contexts, so since there's no
  // reporting in that case we just return early.
  if (!router) {
    return;
  }

  data_controls::ChromeClipboardContext context(source, metadata);

  if (bypassed) {
    router->ReportCopyWarningBypassed(context, verdict);
  } else {
    router->ReportCopy(context, verdict);
  }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

void ReportDragData(const content::ClipboardEndpoint& source,
                    const content::DropData& drop_data,
                    const data_controls::Verdict& verdict) {
  if (drop_data.text) {
    MaybeReportDataControlsCopy(
        source,
        {.size = drop_data.text->size(),
         .format_type = ui::ClipboardFormatType::PlainTextType()},
        verdict);
  }
  if (drop_data.html) {
    MaybeReportDataControlsCopy(
        source,
        {.size = drop_data.html->size(),
         .format_type = ui::ClipboardFormatType::HtmlType()},
        verdict);
  }
  if (!drop_data.file_contents.empty()) {
    // Map `file_contents` to PNG if the image is accessible,
    // otherwise report it as a generic custom type.
    auto type = drop_data.file_contents_image_accessible
                    ? ui::ClipboardFormatType::PngType()
                    : ui::ClipboardFormatType::DataTransferCustomType();
    MaybeReportDataControlsCopy(
        source, {.size = drop_data.file_contents.size(), .format_type = type},
        verdict);
  }
  if (!drop_data.url_infos.empty()) {
    size_t size = 0;
    for (const auto& url_info : drop_data.url_infos) {
      size += url_info.url.spec().size();
    }
    MaybeReportDataControlsCopy(
        source,
        {.size = size, .format_type = ui::ClipboardFormatType::UrlType()},
        verdict);
  }
  if (!drop_data.custom_data.empty()) {
    size_t size = 0;
    for (const auto& item : drop_data.custom_data) {
      size += item.first.size() + item.second.size();
    }
    MaybeReportDataControlsCopy(
        source,
        {.size = size,
         .format_type = ui::ClipboardFormatType::DataTransferCustomType()},
        verdict);
  }
  if (!drop_data.file_system_files.empty()) {
    size_t fs_size = 0;
    for (const auto& fs_file : drop_data.file_system_files) {
      fs_size += fs_file.size;
    }
    MaybeReportDataControlsCopy(
        source,
        {.size = fs_size,
         .format_type = ui::ClipboardFormatType::FilenamesType()},
        verdict);
  }
}

void OnDataControlsPasteWarning(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
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

  // If the data currently being pasted was replaced when it was initially
  // copied from Chrome, replace it back since the warn rule was bypassed. Only do this if
  // `source` has a known browser context to ensure we're not letting through
  // data that was replaced by policies that are no longer applicable due to the
  // profile being closed.
  if (source.browser_context() &&
      metadata.seqno == data_controls::GetLastReplacedClipboardData().seqno) {
    clipboard_paste_data =
        data_controls::GetLastReplacedClipboardData().clipboard_paste_data;
  }

#if BUILDFLAG(IS_ANDROID) || !BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  std::move(callback).Run(std::move(clipboard_paste_data));
#else
  PasteIfAllowedByContentAnalysis(
      destination.web_contents(), source, destination, metadata,
      std::move(clipboard_paste_data), std::move(callback));
#endif  // BUILDFLAG(IS_ANDROID) || !BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
}

data_controls::Verdict GetPasteVerdict(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination) {
  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(destination.browser_context())
                     ->GetPasteVerdict(source, destination);
  if (source.browser_context() &&
      source.browser_context() != destination.browser_context()) {
    verdict = data_controls::Verdict::MergePasteVerdicts(
        data_controls::ChromeRulesServiceFactory::GetInstance()
            ->GetForBrowserContext(source.browser_context())
            ->GetPasteVerdict(source, destination),
        std::move(verdict));
  }
  return verdict;
}

void PasteIfAllowedByDataControls(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  DCHECK(!SkipDataControlOrContentAnalysisChecks(destination));

  auto verdict = GetPasteVerdict(source, destination);
  auto* factory = GetDialogFactory();
  switch (verdict.level()) {
    case data_controls::Rule::Level::kBlock:
      MaybeReportDataControlsPaste(source, destination, metadata, verdict);
      if (factory) {
        factory->ShowDialogIfNeeded(
            destination.web_contents(),
            data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
      }
      std::move(callback).Run(std::nullopt);
      return;
    case data_controls::Rule::Level::kWarn:
      MaybeReportDataControlsPaste(source, destination, metadata, verdict);
      if (factory) {
        factory->ShowDialogIfNeeded(
            destination.web_contents(),
            data_controls::DataControlsDialog::Type::kClipboardPasteWarn,
            base::BindOnce(&OnDataControlsPasteWarning, source, destination,
                           metadata, std::move(verdict),
                           std::move(clipboard_paste_data),
                           std::move(callback)));
      } else {
        std::move(callback).Run(std::nullopt);
      }
      return;
    case data_controls::Rule::Level::kReport:
      MaybeReportDataControlsPaste(source, destination, metadata, verdict);
      break;
    case data_controls::Rule::Level::kAllow:
    case data_controls::Rule::Level::kNotSet:
      break;
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

#if BUILDFLAG(IS_ANDROID) || !BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  std::move(callback).Run(std::move(clipboard_paste_data));
#else
  PasteIfAllowedByContentAnalysis(
      destination.web_contents(), source, destination, metadata,
      std::move(clipboard_paste_data), std::move(callback));
#endif  // BUILDFLAG(IS_ANDROID) || !BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
}

#if !BUILDFLAG(IS_ANDROID)
void OnDlpRulesCheckDone(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
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
#endif  // !BUILDFLAG(IS_ANDROID)

void GetCopyToOSClipboardReplacement(const content::ClipboardEndpoint& source,
                                     std::u16string* replacement) {
  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(source.browser_context())
                     ->GetCopyToOSClipboardVerdict(GetUrlFromEndpoint(source));

  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    *replacement = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE);
  }
}

void IsCopyToOSClipboardRestricted(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  std::u16string replacement;
  GetCopyToOSClipboardReplacement(source, &replacement);
  if (!replacement.empty()) {
    // Before calling `callback`, we remember `data` will correspond to the next
    // clipboard sequence number so that it can be potentially replaced again at
    // paste time.
    data_controls::LastReplacedClipboardDataObserver::GetInstance()
        ->AddDataToNextSeqno(data);
    std::move(callback).Run(metadata.format_type, data, replacement);

    return;
  }

  std::move(callback).Run(metadata.format_type, data, std::nullopt);
}

void OnDataControlsCopyWarning(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    data_controls::Verdict verdict,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback,
    bool bypassed) {
  if (bypassed) {
    MaybeReportDataControlsCopy(source, metadata, verdict, /*bypassed=*/true);
    IsCopyToOSClipboardRestricted(source, metadata, data, std::move(callback));
    return;
  }

  // Once a pending write has been initiated, something must be written to the
  // clipboard to avoid being stuck in a pending write state, which would
  // prevent future writes to the clipboard. Since copying was not allowed,
  // the callback should be run with empty data instead.
  std::move(callback).Run(metadata.format_type, content::ClipboardPasteData(),
                          /*replacement_data=*/std::nullopt);
}

void IsCopyRestrictedByDialog(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback,
    data_controls::DataControlsDialog::Type block_dialog_type,
    data_controls::DataControlsDialog::Type warn_dialog_type) {
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
      factory->ShowDialogIfNeeded(source.web_contents(), block_dialog_type);
    }
    std::move(callback).Run(metadata.format_type, content::ClipboardPasteData(),
                            /*replacement_data=*/std::nullopt);
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
          source.web_contents(), warn_dialog_type,
          base::BindOnce(&OnDataControlsCopyWarning, source, metadata, data,
                         std::move(verdict), std::move(callback)));
    } else {
      std::move(callback).Run(metadata.format_type,
                              content::ClipboardPasteData(),
                              /*replacement_data=*/std::nullopt);
    }
    return;
  }

  if (source_only_verdict.level() == data_controls::Rule::Level::kReport) {
    MaybeReportDataControlsCopy(source, metadata, source_only_verdict);
  }

  IsCopyToOSClipboardRestricted(source, metadata, data, std::move(callback));
}

content::ClipboardEndpoint MakeClipboardEndpoint(
    ui::DataTransferEndpoint dte,
    content::RenderFrameHost* rfh) {
  return content::ClipboardEndpoint(
      dte,
      base::BindRepeating(
          [](content::GlobalRenderFrameHostId rfh_id)
              -> content::BrowserContext* {
            auto* rfh = content::RenderFrameHost::FromID(rfh_id);
            if (!rfh) {
              return nullptr;
            }
            return rfh->GetBrowserContext();
          },
          rfh->GetGlobalId()),
      *rfh);
}

std::optional<content::ClipboardEndpoint> GetValidURLEndpoint(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  auto url = rfh->GetMainFrame()->GetLastCommittedURL();
  if (!url.is_valid()) {
    return std::nullopt;
  }

  ui::DataTransferEndpoint dte(
      url, {.off_the_record = rfh->GetBrowserContext()->IsOffTheRecord()});

  content::ClipboardEndpoint endpoint = MakeClipboardEndpoint(dte, rfh);

  if (SkipDataControlOrContentAnalysisChecks(endpoint)) {
    return std::nullopt;
  }

  return endpoint;
}

}  // namespace

void PasteIfAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  if (SkipDataControlOrContentAnalysisChecks(destination)) {
    std::move(callback).Run(std::nullopt);
    return;
  } else if (base::FeatureList::IsEnabled(
                 data_controls::kEnableClipboardDataControlsAndroid)) {
    // Call PasteIfAllowedByDataControls directly as
    // DataTransferPolicyController::PasteIfAllowed contains logic that isn't
    // relevant to Clank.
    PasteIfAllowedByDataControls(source, destination, metadata,
                                 std::move(clipboard_paste_data),
                                 std::move(callback));
    return;
  } else {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }
#else
  if (ui::DataTransferPolicyController::HasInstance()) {
    std::variant<size_t, std::vector<base::FilePath>> pasted_content;
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
#endif  // BUILDFLAG(IS_ANDROID)
}

void IsClipboardCopyAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          data_controls::kEnableClipboardDataControlsAndroid)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  DCHECK(source.web_contents());
  DCHECK(source.browser_context());

#if !BUILDFLAG(IS_ANDROID)
  // IsUrlAllowedToCopy checks a deprecated CopyPreventionSettings that isn't
  // applicable on Clank.
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

  IsCopyRestrictedByDialog(
      source, metadata, data, std::move(callback),
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock,
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);
}

#if BUILDFLAG(IS_ANDROID)
void IsClipboardShareAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (!base::FeatureList::IsEnabled(
          data_controls::kEnableClipboardDataControlsAndroid)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  DCHECK(source.web_contents());
  DCHECK(source.browser_context());

  IsCopyRestrictedByDialog(
      source, metadata, data, std::move(callback),
      data_controls::DataControlsDialog::Type::kClipboardShareBlock,
      data_controls::DataControlsDialog::Type::kClipboardShareWarn);
}

void IsClipboardGenericCopyActionAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (!base::FeatureList::IsEnabled(
          data_controls::kEnableClipboardDataControlsAndroid)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
    return;
  }

  DCHECK(source.web_contents());
  DCHECK(source.browser_context());

  IsCopyRestrictedByDialog(
      source, metadata, data, std::move(callback),
      data_controls::DataControlsDialog::Type::kClipboardActionBlock,
      data_controls::DataControlsDialog::Type::kClipboardActionWarn);
}
#endif  // BUILDFLAG(IS_ANDROID)

void ReplaceSameTabClipboardDataIfRequiredByPolicy(
    ui::ClipboardSequenceNumberToken seqno,
    content::ClipboardPasteData& data) {
  if (seqno == data_controls::GetLastReplacedClipboardData().seqno) {
    data = data_controls::GetLastReplacedClipboardData().clipboard_paste_data;
  }
}

bool CanPopulateFindBarFromSelection(content::WebContents* web_contents) {
  auto source = GetValidURLEndpoint(web_contents);
  if (!source) {
    return false;
  }

  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(source->browser_context())
                     ->GetCopyToOSClipboardVerdict(GetUrlFromEndpoint(*source));
  return verdict.level() != data_controls::Rule::Level::kBlock;
}

bool IsDragAllowedByPolicy(const content::ClipboardEndpoint& source,
                           const content::DropData& drop_data) {
  if (!base::FeatureList::IsEnabled(
          data_controls::kDataControlsDragEnforcement)) {
    return true;
  }

  if (SkipDataControlOrContentAnalysisChecks(source)) {
    return true;
  }

  // ElapsedTimer is used here to measure the synchronous delay added to the
  // drag operation by the Data Controls policy check.
  base::ElapsedTimer timer;

  auto verdict = data_controls::ChromeRulesServiceFactory::GetInstance()
                     ->GetForBrowserContext(source.browser_context())
                     ->GetCopyToOSClipboardVerdict(GetUrlFromEndpoint(source));

  if (verdict.level() == data_controls::Rule::Level::kBlock ||
      verdict.level() == data_controls::Rule::Level::kWarn ||
      verdict.level() == data_controls::Rule::Level::kReport) {
    ReportDragData(source, drop_data, verdict);
  }

  bool blocked = verdict.level() == data_controls::Rule::Level::kBlock ||
                 verdict.level() == data_controls::Rule::Level::kWarn;

  if (blocked) {
    auto* factory = GetDialogFactory();
    if (factory) {
      factory->ShowDialogIfNeeded(
          source.web_contents(),
          data_controls::DataControlsDialog::Type::kClipboardDragBlock);
    }
  }

  base::UmaHistogramTimes(
      "Enterprise.DataControls.DragAndDrop.EvaluationLatency", timer.Elapsed());

  DataControlsDragAndDropVerdict histogram_verdict;
  switch (verdict.level()) {
    case data_controls::Rule::Level::kBlock:
      histogram_verdict = DataControlsDragAndDropVerdict::kBlocked;
      break;
    case data_controls::Rule::Level::kWarn:
      histogram_verdict = DataControlsDragAndDropVerdict::kWarned;
      break;
    default:
      histogram_verdict = DataControlsDragAndDropVerdict::kAllowed;
      break;
  }
  base::UmaHistogramEnumeration("Enterprise.DataControls.DragAndDrop.Verdict",
                                histogram_verdict);

  return !blocked;
}

bool ReplaceCopyFromFindBar(std::u16string_view selected_text,
                            content::WebContents* web_contents,
                            std::u16string* replacement) {
  CHECK(replacement);
  CHECK(replacement->empty());

  auto source = GetValidURLEndpoint(web_contents);
  if (!source) {
    return false;
  }

  GetCopyToOSClipboardReplacement(*source, replacement);
  if (!replacement->empty()) {
    // Before returning, we persist the data that would have been copied so it
    // can be potentially replaced again at paste time.
    content::ClipboardPasteData data;
    data.text = selected_text;
    data_controls::LastReplacedClipboardDataObserver::GetInstance()
        ->AddDataToNextSeqno(data);
  }
  return !replacement->empty();
}

std::optional<std::u16string> ReplacePasteToFindBar(
    content::WebContents* web_contents) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  CHECK(clipboard);

  auto destination = GetValidURLEndpoint(web_contents);
  if (!destination) {
    return std::nullopt;
  }

  std::optional<ui::DataTransferEndpoint> source_dte =
      clipboard->GetSource(ui::ClipboardBuffer::kCopyPaste);
  auto source = content::GetSourceClipboardEndpoint(
      base::OptionalToPtr(source_dte), ui::ClipboardBuffer::kCopyPaste);

  auto verdict = GetPasteVerdict(source, *destination);

  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    // On a blocked verdict, the find bar is not allowed to get replaced data
    // tracked internally by the browser.
    return std::nullopt;
  }

  const ui::ClipboardSequenceNumberToken& seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);

  if (source.browser_context() &&
      seqno == data_controls::GetLastReplacedClipboardData().seqno) {
    return data_controls::GetLastReplacedClipboardData()
        .clipboard_paste_data.text;
  }

  return std::nullopt;
}

}  // namespace enterprise_data_protection
