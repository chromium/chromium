// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/ozone_wayland_state_dump_source.h"

#include <memory>
#include <sstream>

#include "ui/ozone/public/ozone_platform.h"

namespace system_logs {

OzoneWaylandStateDumpSource::OzoneWaylandStateDumpSource()
    : SystemLogsSource("OzoneWaylanStateDump") {}

void OzoneWaylandStateDumpSource::Fetch(SysLogsSourceCallback callback) {
  std::ostringstream out;
  ui::OzonePlatform::GetInstance()->DumpState(out);
  auto response = std::make_unique<SystemLogsResponse>();
  response->emplace("ozone-wayland-state", out.str());
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
