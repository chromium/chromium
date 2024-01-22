// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_dialog_utils.h"

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_elider.h"
#include "ui/shell_dialogs/selected_file_info.h"

// static
download::DownloadItem* DownloadDialogUtils::FindAndRemoveDownload(
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>* downloads,
    const std::string& download_guid) {
  auto iter = base::ranges::find(*downloads, download_guid,
                                 &download::DownloadItem::GetGuid);

  if (iter == downloads->end())
    return nullptr;

  download::DownloadItem* result = *iter;
  downloads->erase(iter);
  return result;
}

// static
void DownloadDialogUtils::CreateNewFileDone(
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    download::PathValidationResult result,
    const base::FilePath& target_path) {
  if (download::IsPathValidationSuccessful(result)) {
    std::move(callback).Run(DownloadConfirmationResult::CONFIRMED,
                            ui::SelectedFileInfo(target_path));
  } else {
    std::move(callback).Run(DownloadConfirmationResult::FAILED,
                            ui::SelectedFileInfo());
  }
}

// static
std::string DownloadDialogUtils::GetDisplayURLForPageURL(const GURL& page_url) {
  // The URL could be very long, especially since we are including query
  // parameters, path, etc.  Elide the URL to a shorter length because the
  // infobar cannot handle scrolling and completely obscures Chrome if the text
  // is too long.
  //
  // 150 was chosen as it does not cause the infobar to overrun the screen on a
  // test Android One device with 480 x 854 resolution.  At this resolution the
  // infobar covers approximately 2/3 of the screen, and all controls are still
  // visible.
  //
  // TODO(dewittj): Display something better than an elided URL string in the
  // infobar.
  const size_t kMaxLengthOfDisplayedPageUrl = 150;

  std::u16string formatted_url = url_formatter::FormatUrl(page_url);
  std::u16string elided_url;
  gfx::ElideString(formatted_url, kMaxLengthOfDisplayedPageUrl, &elided_url);
  return base::UTF16ToUTF8(elided_url);
}
