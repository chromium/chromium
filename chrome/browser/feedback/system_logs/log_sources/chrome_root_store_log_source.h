// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_ROOT_STORE_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_ROOT_STORE_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Get information about the contents of the Chrome Root Store in use.
class ChromeRootStoreLogSource : public system_logs::SystemLogsSource {
 public:
  ChromeRootStoreLogSource();

  ChromeRootStoreLogSource(const ChromeRootStoreLogSource&) = delete;
  ChromeRootStoreLogSource& operator=(const ChromeRootStoreLogSource&) = delete;

  ~ChromeRootStoreLogSource() override;

  // SystemLogsSource override.
  void Fetch(system_logs::SysLogsSourceCallback callback) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_ROOT_STORE_LOG_SOURCE_H_
