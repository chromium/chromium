// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DOWNLOADS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DOWNLOADS_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"

namespace download {
class DownloadItem;
}

namespace enterprise_connectors {

// A ContentAnalysisDelegateBase implementation meant to be used to display the
// ContentAnalysisDialog for a download that triggered a block or a warning, and
// for which a custom message must be shown to the user.
class ContentAnalysisDownloadsDelegate : public ContentAnalysisDelegateBase {
 public:
  ContentAnalysisDownloadsDelegate(
      const std::u16string& filename,
      const std::u16string& custom_message,
      GURL custom_learn_more_url,
      bool bypass_justification_required,
      base::OnceCallback<void()> open_file_callback,
      base::OnceCallback<void()> discard_file_callback,
      download::DownloadItem* download_item);
  ~ContentAnalysisDownloadsDelegate() override;

  // Called when the user opts to keep the download and open it. Should not be
  // called if the result was a "block" since the option shouldn't be available
  // in that case.
  void BypassWarnings(
      absl::optional<std::u16string> user_justification) override;

  // Called when the user opts to delete the downloaded file and not open it.
  void Cancel(bool warning) override;

  absl::optional<std::u16string> GetCustomMessage() const override;

  absl::optional<GURL> GetCustomLearnMoreUrl() const override;

  bool BypassRequiresJustification() const override;
  std::u16string GetBypassJustificationLabel() const override;

  absl::optional<std::u16string> OverrideCancelButtonText() const override;

 private:
  // Resets |open_file_callback_| and |discard_file_callback_|, ensuring actions
  // can't be attempted on a file that has already been opened or discarded
  // (which may be undefined).
  void ResetCallbacks();

  std::u16string filename_;
  std::u16string custom_message_;
  GURL custom_learn_more_url_;
  bool bypass_justification_required_;
  base::OnceCallback<void()> open_file_callback_;
  base::OnceCallback<void()> discard_file_callback_;
  raw_ptr<download::DownloadItem> download_item_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DOWNLOADS_DELEGATE_H_
