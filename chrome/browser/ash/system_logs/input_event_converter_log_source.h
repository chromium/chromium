// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_INPUT_EVENT_CONVERTER_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_INPUT_EVENT_CONVERTER_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches the description of the global ozone input controller for the system
// logs.
class InputEventConverterLogSource : public SystemLogsSource {
 public:
  InputEventConverterLogSource();
  InputEventConverterLogSource(const InputEventConverterLogSource&) = delete;
  InputEventConverterLogSource& operator=(const InputEventConverterLogSource&) =
      delete;
  ~InputEventConverterLogSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_INPUT_EVENT_CONVERTER_LOG_SOURCE_H_
