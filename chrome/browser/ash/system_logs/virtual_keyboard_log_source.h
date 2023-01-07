// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_VIRTUAL_KEYBOARD_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_VIRTUAL_KEYBOARD_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

class VirtualKeyboardLogSource : public SystemLogsSource {
 public:
  VirtualKeyboardLogSource();
  ~VirtualKeyboardLogSource() override = default;

  VirtualKeyboardLogSource(const VirtualKeyboardLogSource&) = delete;
  VirtualKeyboardLogSource& operator=(const VirtualKeyboardLogSource&) = delete;

 private:
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_VIRTUAL_KEYBOARD_LOG_SOURCE_H_
