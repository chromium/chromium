// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"

namespace webapk {

class WebApkRestoreManager;

// Task for installing previously synced WebAPK on new devices. Each instance
// represents a WebAPK to be install.
class WebApkRestoreTask {
 public:
  using CompleteCallback = base::OnceCallback<void(const GURL&)>;

  explicit WebApkRestoreTask(base::PassKey<WebApkRestoreManager>,
                             const sync_pb::WebApkSpecifics& webapk_specifics,
                             CompleteCallback complete_callback);
  WebApkRestoreTask(const WebApkRestoreTask&) = delete;
  WebApkRestoreTask& operator=(const WebApkRestoreTask&) = delete;
  ~WebApkRestoreTask();

  void Start();

 private:
  CompleteCallback complete_callback_;
  std::unique_ptr<webapps::ShortcutInfo> fallback_info_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
