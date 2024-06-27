// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_net_logs_connection.h"

#include <unistd.h>

#include <string>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwNetLogsConnection_jni.h"

using base::android::JavaParamRef;

namespace android_webview {

AwNetLogsConnection::AwNetLogsConnection() = default;

AwNetLogsConnection::~AwNetLogsConnection() {
  if (aw_net_log_observer_) {
    aw_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
  }
}

void AwNetLogsConnection::startNetLogBounded(int file_descriptor) {
  // Do nothing if already logging to a directory.
  if (aw_net_log_observer_) {
    return;
  }

  aw_net_log_observer_ = net::FileNetLogObserver::CreateBoundedFile(
      base::File(file_descriptor), max_file_size,
      net::NetLogCaptureMode::kDefault, /*constants=*/nullptr);
  aw_net_log_observer_->StartObserving(net::NetLog::Get());
}

void AwNetLogsConnection::stopNetLogs() {
  if (!aw_net_log_observer_) {
    return;
  }
  aw_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
}

AwNetLogsConnection* GetInstance() {
  static AwNetLogsConnection* instance = nullptr;
  if (!instance) {
    instance = new AwNetLogsConnection();
  }
  return instance;
}

static void JNI_AwNetLogsConnection_StartNetLogs(JNIEnv* env, const jint j_fd) {
  GetInstance()->startNetLogBounded(j_fd);
}

static void JNI_AwNetLogsConnection_StopNetLogs(JNIEnv* env) {
  GetInstance()->stopNetLogs();
}

}  // namespace android_webview
