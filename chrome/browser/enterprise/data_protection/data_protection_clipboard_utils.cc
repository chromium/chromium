// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

namespace enterprise_data_protection {

void HandleExpandedPaths(
    std::unique_ptr<enterprise_connectors::FilesScanData> fsd,
    base::WeakPtr<content::WebContents> web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
    std::vector<base::FilePath> paths,
    content::ContentBrowserClient::IsClipboardPasteContentAllowedCallback
        callback) {
  if (!web_contents) {
    return;
  }

  dialog_data.paths = fsd->expanded_paths();
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents.get(), std::move(dialog_data),
      base::BindOnce(
          [](std::unique_ptr<enterprise_connectors::FilesScanData> fsd,
             std::vector<base::FilePath> paths,
             content::ContentBrowserClient::
                 IsClipboardPasteContentAllowedCallback callback,
             const enterprise_connectors::ContentAnalysisDelegate::Data& data,
             enterprise_connectors::ContentAnalysisDelegate::Result& result) {
            absl::optional<content::ClipboardPasteData> clipboard_paste_data;
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
              clipboard_paste_data = content::ClipboardPasteData(
                  std::string(), std::string(), std::move(allowed_paths));
            }
            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(fsd), std::move(paths), std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PASTE);
}

void HandleStringData(
    content::WebContents* web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
    content::ContentBrowserClient::IsClipboardPasteContentAllowedCallback
        callback) {
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents, std::move(dialog_data),
      base::BindOnce(
          [](content::ContentBrowserClient::
                 IsClipboardPasteContentAllowedCallback callback,
             const enterprise_connectors::ContentAnalysisDelegate::Data& data,
             enterprise_connectors::ContentAnalysisDelegate::Result& result) {
            if (!result.text_results[0] && !result.image_result) {
              std::move(callback).Run(absl::nullopt);
              return;
            }

            content::ClipboardPasteData clipboard_paste_data;
            if (result.text_results[0]) {
              clipboard_paste_data.text = data.text[0];
            }
            if (result.image_result) {
              clipboard_paste_data.image = data.image;
            }
            std::move(callback).Run(std::move(clipboard_paste_data));
          },
          std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PASTE);
}

}  // namespace enterprise_data_protection
