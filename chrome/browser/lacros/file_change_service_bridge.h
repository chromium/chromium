// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_H_

#include "base/callback_list.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class Profile;

namespace storage {
class FileSystemURL;
}  // namespace storage

// The bridge implemented in Lacros which is connected to the
// `FileChangeServiceBridgeAsh` in Ash via crosapi. This bridge enables file
// change events originating from Lacros to be propagated to the
// `FileChangeService`, and its observers, in Ash.
class FileChangeServiceBridge : public KeyedService {
 public:
  explicit FileChangeServiceBridge(Profile* profile);
  FileChangeServiceBridge(const FileChangeServiceBridge&) = delete;
  FileChangeServiceBridge& operator=(const FileChangeServiceBridge&) = delete;
  ~FileChangeServiceBridge() override;

 private:
  // Invoked when a file has been created at `url` in fulfillment of a
  // `window.showSaveFilePicker()` request from the given
  // `file_picker_binding_context`.
  //
  // See `content::FileSystemAccessEntryFactory::BindingContext`.
  void OnFileCreatedFromShowSaveFilePicker(
      const GURL& file_picker_binding_context,
      const storage::FileSystemURL& url);

  // Subscription to be notified of file creation events originating from
  // `window.showSaveFilePicker()`.
  base::CallbackListSubscription
      file_created_from_show_save_file_picker_subscription_;
};

#endif  // CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_H_
