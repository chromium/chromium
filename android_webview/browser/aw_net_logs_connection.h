// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_NET_LOGS_CONNECTION_H_
#define ANDROID_WEBVIEW_BROWSER_AW_NET_LOGS_CONNECTION_H_

#include <stdint.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "net/log/file_net_log_observer.h"

namespace android_webview {

class AwNetLogsConnection {
 public:
  AwNetLogsConnection();
  ~AwNetLogsConnection();
  void startNetLogBounded(int file_descriptor);

  void stopNetLogs();

 private:
  std::unique_ptr<net::FileNetLogObserver> aw_net_log_observer_;

  // Max file sizr of 100Mb.
  uint64_t max_file_size = 100000000;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_NET_LOGS_CONNECTION_H_
