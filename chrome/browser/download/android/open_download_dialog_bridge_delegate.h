// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/android/open_download_dialog_bridge.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"

// Class for showing dialogs to asks whether user wants to open a downloaded
// file from an external app.
class OpenDownloadDialogBridgeDelegate
    : public download::DownloadItem::Observer {
 public:
  OpenDownloadDialogBridgeDelegate();
  OpenDownloadDialogBridgeDelegate(const OpenDownloadDialogBridgeDelegate&) =
      delete;
  OpenDownloadDialogBridgeDelegate& operator=(
      const OpenDownloadDialogBridgeDelegate&) = delete;

  ~OpenDownloadDialogBridgeDelegate() override;

  // Called to create and show a dialog for opening a download.
  void CreateDialog(download::DownloadItem* download_item);

  // Called from Java via JNI.
  void OnConfirmed(const std::string& download_guid, bool accepted);

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  // Download items that are requesting the dialog. Could get deleted while
  // the dialog is showing.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items_;

  std::unique_ptr<OpenDownloadDialogBridge> open_download_dialog_bridge_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_DELEGATE_H_
