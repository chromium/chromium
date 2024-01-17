// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/open_download_dialog_bridge_delegate.h"

#include <string>

#include "base/android/path_utils.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;

OpenDownloadDialogBridgeDelegate::OpenDownloadDialogBridgeDelegate()
    : open_download_dialog_bridge_(
          std::make_unique<OpenDownloadDialogBridge>(this)) {}

OpenDownloadDialogBridgeDelegate::~OpenDownloadDialogBridgeDelegate() {
  for (download::DownloadItem* download_item : download_items_) {
    download_item->RemoveObserver(this);
  }
}

void OpenDownloadDialogBridgeDelegate::CreateDialog(
    download::DownloadItem* download_item) {
  // Don't shown duplicate dialog again if it is already showing.
  if (base::Contains(download_items_, download_item)) {
    return;
  }
  download_item->AddObserver(this);
  download_items_.push_back(download_item);

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download_item);

  open_download_dialog_bridge_->Show(
      Profile::FromBrowserContext(browser_context), download_item->GetGuid());
}

void OpenDownloadDialogBridgeDelegate::OnConfirmed(
    const std::string& download_guid,
    bool accepted) {
  download::DownloadItem* download = DownloadDialogUtils::FindAndRemoveDownload(
      &download_items_, download_guid);
  if (!download) {
    return;
  }
  download->RemoveObserver(this);

  if (accepted) {
    download->OpenDownload();
  }
}

void OpenDownloadDialogBridgeDelegate::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  auto iter = base::ranges::find(download_items_, download_item);
  if (iter != download_items_.end()) {
    download_items_.erase(iter);
  }
}
