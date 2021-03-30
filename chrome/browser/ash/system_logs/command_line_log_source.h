// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_COMMAND_LINE_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_COMMAND_LINE_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// This class gathers logs data from various command line tools which we can
// not access using Debug Daemon.
class CommandLineLogSource : public SystemLogsSource {
 public:
  CommandLineLogSource();
  ~CommandLineLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandLineLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_COMMAND_LINE_LOG_SOURCE_H_
