// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"

namespace ash {

SystemWebAppBackgroundTaskInfo::~SystemWebAppBackgroundTaskInfo() = default;
SystemWebAppBackgroundTaskInfo::SystemWebAppBackgroundTaskInfo() = default;

SystemWebAppBackgroundTaskInfo::SystemWebAppBackgroundTaskInfo(
    const SystemWebAppBackgroundTaskInfo& other) = default;

SystemWebAppBackgroundTaskInfo::SystemWebAppBackgroundTaskInfo(
    const std::optional<base::TimeDelta>& period,
    const GURL& url,
    bool open_immediately)
    : period(period), url(url), open_immediately(open_immediately) {}

}  // namespace ash
