// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/rename_result.h"

// Contains various utility methods for conversions between DownloadItem and
// OfflineItem.
class OfflineItemUtils {
  using DownloadRenameResult = download::DownloadItem::DownloadRenameResult;

 public:
  static offline_items_collection::OfflineItem CreateOfflineItem(
      const std::string& name_space,
      download::DownloadItem* item);

  static std::string GetDownloadNamespacePrefix(bool is_off_the_record);

  static bool IsDownload(const offline_items_collection::ContentId& id);

  // Converts DownloadInterruptReason to offline_items_collection::FailState.
  static offline_items_collection::FailState
  ConvertDownloadInterruptReasonToFailState(
      download::DownloadInterruptReason reason);

  // Converts offline_items_collection::FailState to DownloadInterruptReason.
  static download::DownloadInterruptReason
  ConvertFailStateToDownloadInterruptReason(
      offline_items_collection::FailState fail_state);

  // Gets the short text to display for a offline_items_collection::FailState.
  static base::string16 GetFailStateMessage(
      offline_items_collection::FailState fail_state);

  // Converts download::DownloadItem::DownloadRenameResult to
  // offline_items_collection::RenameResult.
  static RenameResult ConvertDownloadRenameResultToRenameResult(
      DownloadRenameResult download_rename_result);

 private:
  DISALLOW_COPY_AND_ASSIGN(OfflineItemUtils);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_UTILS_H_
