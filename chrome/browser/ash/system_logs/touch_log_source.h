// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

class TouchLogSource : public SystemLogsSource {
 public:
  TouchLogSource() : SystemLogsSource("Touch") {}
  ~TouchLogSource() override {}

 private:
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(TouchLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_
