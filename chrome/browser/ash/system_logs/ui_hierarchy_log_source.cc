// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/ui_hierarchy_log_source.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/debug_utils.h"

namespace system_logs {

void UiHierarchyLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  {
    std::ostringstream out;
    ash::debug::PrintWindowHierarchy(&out, scrub_data_);
    response->emplace("UI Hierarchy: Windows", out.str());
  }

  {
    std::ostringstream out;
    ash::debug::PrintViewHierarchy(&out);
    response->emplace("UI Hierarchy: Views", out.str());
  }

  {
    std::ostringstream out;
    ash::debug::PrintLayerHierarchy(&out);
    response->emplace("UI Hierarchy: Layers", out.str());
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
