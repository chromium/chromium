// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_UI_HIERARCHY_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_UI_HIERARCHY_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

class UiHierarchyLogSource : public SystemLogsSource {
 public:
  explicit UiHierarchyLogSource(bool scrub_data)
      : SystemLogsSource("UiHierarchy"), scrub_data_(scrub_data) {}
  UiHierarchyLogSource(const UiHierarchyLogSource&) = delete;
  UiHierarchyLogSource& operator=(const UiHierarchyLogSource&) = delete;
  ~UiHierarchyLogSource() override = default;

 private:
  // Overridden from SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

  const bool scrub_data_;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_UI_HIERARCHY_LOG_SOURCE_H_
