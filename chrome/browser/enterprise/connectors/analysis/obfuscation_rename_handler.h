// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_OBFUSCATION_RENAME_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_OBFUSCATION_RENAME_HANDLER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_item_rename_handler.h"

namespace download {
class DownloadItem;
}

namespace enterprise_obfuscation {

// Rename handler used on ChromeOS for downloads obfuscated during enterprise
// content analysis that were staged in a local temporary directory before being
// moved to their final target path on virtual/cloud filesystems (e.g. OneDrive,
// Google Drive).
class ObfuscationRenameHandler : public download::DownloadItemRenameHandler {
 public:
  static std::unique_ptr<ObfuscationRenameHandler> CreateIfNeeded(
      download::DownloadItem* download_item);

  explicit ObfuscationRenameHandler(download::DownloadItem* download_item);
  ~ObfuscationRenameHandler() override;

  // download::DownloadItemRenameHandler:
  void Start(ProgressCallback progress_callback,
             RenameCallback rename_callback) override;
  bool ShowRenameProgress() override;

 private:
  void OnMoveComplete(RenameCallback rename_callback,
                      const base::FilePath& destination_path,
                      bool success);

  base::WeakPtrFactory<ObfuscationRenameHandler> weak_factory_{this};
};

}  // namespace enterprise_obfuscation

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_OBFUSCATION_RENAME_HANDLER_H_
