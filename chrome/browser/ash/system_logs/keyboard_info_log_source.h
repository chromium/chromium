// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_KEYBOARD_INFO_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_KEYBOARD_INFO_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

class KeyboardInfoLogSource : public SystemLogsSource {
 public:
  KeyboardInfoLogSource();
  ~KeyboardInfoLogSource() override = default;

  KeyboardInfoLogSource(const KeyboardInfoLogSource&) = delete;
  KeyboardInfoLogSource& operator=(const KeyboardInfoLogSource&) = delete;

 private:
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_KEYBOARD_INFO_LOG_SOURCE_H_
