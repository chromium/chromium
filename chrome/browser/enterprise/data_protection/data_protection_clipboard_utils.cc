// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/rules_service.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/enterprise/content/clipboard_restriction_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_data_protection {

namespace {

void HandleExpandedPaths(
    std::unique_ptr<enterprise_connectors::FilesScanData> fsd,
    base::WeakPtr<content::WebContents> web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
    std::vector<base::FilePath> paths,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  if (!web_contents) {
    return;
  }

  dialog_data.paths = fsd->expanded_paths();
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents.get(), std::move(dialog_data),
      base::BindOnce(
          [](std::unique_ptr<enterprise_connectors::FilesScanData> fsd,
             std::vector<base::FilePath> paths,
             content::ContentBrowserClient::IsClipboardPasteAllowedCallback
                 callback,
             const enterprise_connectors::ContentAnalysisDelegate::Data& data,
             enterprise_connectors::ContentAnalysisDelegate::Result& result) {
            std::optional<content::ClipboardPasteData> clipboard_paste_data;
            auto blocked = fsd->IndexesToBlock(result.paths_results);
            if (blocked.size() != paths.size()) {
              std::vector<base::FilePath> allowed_paths;
              allowed_paths.reserve(paths.size());
              for (size_t i = 0; i < paths.size(); ++i) {
                if (base::Contains(blocked, i)) {
                  result.paths_results[i] = false;
                } else {
                  allowed_paths.push_back(paths[i]);
                  DCHECK(result.paths_results[i]);
                }
              }
              clipboard_paste_data = content::ClipboardPasteData();
              clipboard_paste_data->file_paths = std::move(allowed_paths);
            }
            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(fsd), std::move(paths), std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PASTE);
}

void HandleStringData(
    content::WebContents* web_contents,
    content::ClipboardPasteData clipboard_paste_data,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
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

bool SkipDataControlOrContentAnalysisChecks(
    const content::ClipboardEndpoint& main_endpoint) {
  // Data Controls and content analysis copy/paste checks require an active tab
  // to be meaningful, so if it's gone they can be skipped.
  auto* web_contents = main_endpoint.web_contents();
  if (!web_contents) {
    return true;
  }

  // Data Controls and content analysis copy/paste checks are only meaningful in
  // Chrome tabs, so they should always be skipped for source-only checks (ex.
  // copy prevention checks).
  if (!main_endpoint.data_transfer_endpoint().has_value() ||
      !main_endpoint.data_transfer_endpoint()->IsUrlType()) {
    return true;
  }

  return false;
}

void PasteIfAllowedByContentAnalysis(
    content::WebContents* web_contents,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  DCHECK(web_contents);
  DCHECK(!SkipDataControlOrContentAnalysisChecks(destination));

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
          profile, *destination.data_transfer_endpoint()->GetURL(),
          &dialog_data, connector)) {
    std::move(callback).Run(std::move(clipboard_paste_data));
    return;
  }

  dialog_data.reason =
      enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE;

  if (is_files) {
    auto paths = std::move(clipboard_paste_data.file_paths);
    auto fsd = std::make_unique<enterprise_connectors::FilesScanData>(paths);
    auto* fsd_ptr = fsd.get();
    fsd_ptr->ExpandPaths(base::BindOnce(&HandleExpandedPaths, std::move(fsd),
                                        web_contents->GetWeakPtr(),
                                        std::move(dialog_data), connector,
                                        std::move(paths), std::move(callback)));
  } else {
    dialog_data.AddClipboardData(clipboard_paste_data);
    HandleStringData(web_contents, std::move(clipboard_paste_data),
                     std::move(dialog_data), connector, std::move(callback));
  }
}

void PasteIfAllowedByDataControls(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
  DCHECK(!SkipDataControlOrContentAnalysisChecks(destination));

  auto verdict = data_controls::RulesServiceFactory::GetForBrowserContext(
                     destination.browser_context())
                     ->GetPasteVerdict(source, destination, metadata);
  if (source.browser_context() &&
      source.browser_context() != destination.browser_context()) {
    verdict = data_controls::Verdict::Merge(
        data_controls::RulesServiceFactory::GetForBrowserContext(
            source.browser_context())
            ->GetPasteVerdict(source, destination, metadata),
        std::move(verdict));
  }

  // TODO(b/302340176): Add support for verdicts other than "block".
  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    data_controls::DataControlsDialog::Show(
        destination.web_contents(),
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    std::move(callback).Run(std::nullopt);
    return;
  }

  PasteIfAllowedByContentAnalysis(destination.web_contents(), destination,
                                  metadata, std::move(clipboard_paste_data),
                                  std::move(callback));
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
    const std::u16string& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(data, std::nullopt);
    return;
  }

  auto verdict = data_controls::RulesServiceFactory::GetForBrowserContext(
                     source.browser_context())
                     ->GetCopyToOSClipboardVerdict(
                         *source.data_transfer_endpoint()->GetURL());

  // TODO(b/302340176): Add support for verdicts other than "block".
  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    std::u16string replacement_data = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE);
    std::move(callback).Run(data, std::move(replacement_data));
    return;
  }

  std::move(callback).Run(data, std::nullopt);
}

void IsCopyRestrictedByDialog(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const std::u16string& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (SkipDataControlOrContentAnalysisChecks(source)) {
    std::move(callback).Run(data, std::nullopt);
    return;
  }

  auto verdict = data_controls::RulesServiceFactory::GetForBrowserContext(
                     source.browser_context())
                     ->GetCopyRestrictedBySourceVerdict(
                         *source.data_transfer_endpoint()->GetURL());

  // TODO(b/302340176): Add support for verdicts other than "block".
  // TODO(b/303640183): Add reporting logic.
  if (verdict.level() == data_controls::Rule::Level::kBlock) {
    data_controls::DataControlsDialog::Show(
        source.web_contents(),
        data_controls::DataControlsDialog::Type::kClipboardCopyBlock);
    return;
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
    const std::u16string& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  DCHECK(source.web_contents());
  DCHECK(source.browser_context());
  DCHECK(source.data_transfer_endpoint());
  DCHECK(source.data_transfer_endpoint()->IsUrlType());
  const GURL& url = *source.data_transfer_endpoint()->GetURL();

  std::u16string replacement_data;
  ClipboardRestrictionService* service =
      ClipboardRestrictionServiceFactory::GetInstance()->GetForBrowserContext(
          source.browser_context());
  if (!service->IsUrlAllowedToCopy(url, metadata.size.value_or(0),
                                   &replacement_data)) {
    std::move(callback).Run(data, std::move(replacement_data));
    return;
  }

  IsCopyRestrictedByDialog(source, metadata, data, std::move(callback));
}

}  // namespace enterprise_data_protection
