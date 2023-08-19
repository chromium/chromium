// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_WAYLAND_STATE_DUMP_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_WAYLAND_STATE_DUMP_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches Ozone wayland state dump.
class OzoneWaylandStateDumpSource : public SystemLogsSource {
 public:
  OzoneWaylandStateDumpSource();
  OzoneWaylandStateDumpSource(const OzoneWaylandStateDumpSource&) = delete;
  OzoneWaylandStateDumpSource& operator=(const OzoneWaylandStateDumpSource&) =
      delete;
  ~OzoneWaylandStateDumpSource() override = default;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_WAYLAND_STATE_DUMP_SOURCE_H_
