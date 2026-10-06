// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/android/pdf_content_fetcher_android.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace page_content_annotations {

namespace {

std::optional<PdfResult> ReadPdfFileOnBackgroundThread(
    const std::string& filepath,
    uint32_t size_limit,
    url::Origin origin) {
  base::FilePath path(filepath);
  // If `filepath` is passed as a `file://` URI rather than a raw filesystem
  // path, convert it to a `base::FilePath` and filter out invalid file URLs.
  if (GURL file_url(filepath); file_url.is_valid() && file_url.SchemeIsFile()) {
    if (!net::FileURLToFilePath(file_url, &path)) {
      return std::nullopt;
    }
  }
  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (!file_size.has_value()) {
    return std::nullopt;
  }
  if (file_size.value() > static_cast<int64_t>(size_limit)) {
    return PdfResult(std::move(origin));
  }
  std::optional<std::vector<uint8_t>> bytes = base::ReadFileToBytes(path);
  if (!bytes.has_value()) {
    return std::nullopt;
  }
  // The file size can change between `GetFileSize()` and `ReadFileToBytes()`,
  // e.g. for a `content://` URI backed by a streaming `ContentProvider`, so
  // check the size limit again after reading.
  if (bytes->size() > size_limit) {
    return PdfResult(std::move(origin));
  }
  return PdfResult(std::move(origin), std::move(*bytes));
}

}  // namespace

void FetchPdfContentForWebContentsAndroid(
    content::WebContents& web_contents,
    const PdfOptions& options,
    FetchPdfContentResultCallback callback) {
  if (options.format() == PdfOptions::Format::kBytes) {
    TabAndroid* tab_android = TabAndroid::FromWebContents(&web_contents);
    // Incognito PDFs are backed by an in-memory file (a `/proc/` path to a
    // memfd), which may not be readable reliably. Glic does not run in
    // incognito, so do not support it.
    if (tab_android && !tab_android->IsIncognito() && tab_android->IsPdf()) {
      std::string filepath = tab_android->GetCanonicalFilepath();
      if (!filepath.empty()) {
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            // `GetLastCommittedURL()` returns the virtual URL, which is the
            // download URL for inline web PDFs (rather than the opaque
            // `chrome-native://pdf/link?url=...` URL), so web PDFs keep their
            // origin while local PDFs get an opaque origin.
            base::BindOnce(
                &ReadPdfFileOnBackgroundThread, std::move(filepath),
                options.size_limit(),
                url::Origin::Create(web_contents.GetLastCommittedURL())),
            std::move(callback));
        return;
      }
    }
  }
  std::move(callback).Run(std::nullopt);
}

}  // namespace page_content_annotations
