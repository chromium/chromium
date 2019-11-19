// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "components/download/public/common/download_item.h"

namespace safe_browsing {

// This class implements the BinaryUploadService::Request interface for a
// particular DownloadItem. It is neither moveable nor copyable.
class DownloadItemRequest : public BinaryUploadService::Request,
                            download::DownloadItem::Observer {
 public:
  // Create a DownloadItemRequest for the given |item|. If |read_immediately| is
  // true, try to read the file contents right away. Otherwise, wait until the
  // file has been renamed to its final path. If the caller expects |item| to be
  // renamed imminently, it's recommended to set |read_immediately| to false,
  // to avoid race conditions while reading the file.
  DownloadItemRequest(download::DownloadItem* item,
                      bool read_immediately,
                      BinaryUploadService::Callback callback);
  ~DownloadItemRequest() override;

  DownloadItemRequest(const DownloadItemRequest&) = delete;
  DownloadItemRequest& operator=(const DownloadItemRequest&) = delete;
  DownloadItemRequest(DownloadItemRequest&&) = delete;
  DownloadItemRequest& operator=(DownloadItemRequest&&) = delete;

  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

  // download::DownloadItem::Observer implementation.
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  void ReadFile();

  void OnGotFileContents(std::string contents);

  // Calls to GetFileContents can be deferred if the download item is not yet
  // renamed to its final location. When ready, this method runs those
  // callbacks.
  void RunPendingGetFileContentsCallbacks();

  // Pointer the download item for upload. This must be accessed only the UI
  // thread. Unowned.
  download::DownloadItem* item_;

  // The file's data.
  Data data_;

  // Is the |data_| member valid?  It becomes valid once the file has been
  // read successfully.
  bool is_data_valid_ = false;

  // All pending callbacks to GetFileContents before the download item is ready.
  std::vector<DataCallback> pending_callbacks_;

  base::WeakPtrFactory<DownloadItemRequest> weakptr_factory_;
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_
