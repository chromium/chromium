// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DATA_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DATA_H_

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/values.h"

namespace ash {

// An in-memory representation of a System Web App registered in
// `WebAppDatabase`. (Also see the corresponding on-disk representation:
// `SystemWebAppDataProto`). This `WebAppRegistry` entry helps to identify a
// System Web App during reinstall, i.e. before
// `SystemWebAppManager::OnAppsSynchronized` is called.
struct SystemWebAppData {
  base::Value AsDebugValue() const;

  SystemWebAppType system_app_type;
};

bool operator==(const SystemWebAppData& data1, const SystemWebAppData& data2);
bool operator!=(const SystemWebAppData& data1, const SystemWebAppData& data2);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DATA_H_
