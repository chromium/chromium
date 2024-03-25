// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "url/gurl.h"

namespace webapk {

WebApkRestoreTask::WebApkRestoreTask(
    base::PassKey<WebApkRestoreManager> pass_key,
    const sync_pb::WebApkSpecifics& webapk_specifics,
    CompleteCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
  fallback_info_ = std::make_unique<webapps::ShortcutInfo>(
      GURL(webapk_specifics.start_url()));
  fallback_info_->manifest_id = GURL(webapk_specifics.manifest_id());
  fallback_info_->scope = GURL(webapk_specifics.scope());
  fallback_info_->user_title = base::UTF8ToUTF16(webapk_specifics.name());
  fallback_info_->name = fallback_info_->user_title;
  fallback_info_->short_name = fallback_info_->user_title;
}

WebApkRestoreTask::~WebApkRestoreTask() = default;

void WebApkRestoreTask::Start() {
  // TODO(crbug.com/41496289): Add real implementation.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(complete_callback_),
                                fallback_info_->manifest_id));
}

}  // namespace webapk
