// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"

namespace content {
class WebContents;
}  // namespace content

// Class for showing dialogs to asks whether user wants to download a file
// that already exists on disk
class DuplicateDownloadDialogBridgeDelegate
    : public download::DownloadItem::Observer {
 public:
  // static
  static DuplicateDownloadDialogBridgeDelegate* GetInstance();

  DuplicateDownloadDialogBridgeDelegate();
  DuplicateDownloadDialogBridgeDelegate(
      const DuplicateDownloadDialogBridgeDelegate&) = delete;
  DuplicateDownloadDialogBridgeDelegate& operator=(
      const DuplicateDownloadDialogBridgeDelegate&) = delete;

  ~DuplicateDownloadDialogBridgeDelegate() override;

  // Called to create and show a dialog for a duplicate download.
  void CreateDialog(download::DownloadItem* download_item,
                    const base::FilePath& file_path,
                    content::WebContents* web_contents,
                    DownloadTargetDeterminerDelegate::ConfirmationCallback
                        file_selected_callback);

  // Called from Java via JNI.
  void OnConfirmed(
      const std::string& download_guid,
      const base::FilePath& file_path,
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      bool accepted);

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  // Download items that are requesting the dialog. Could get deleted while
  // the dialog is showing.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items_;

  base::WeakPtrFactory<DuplicateDownloadDialogBridgeDelegate> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_
