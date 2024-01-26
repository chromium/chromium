// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_SEA_PEN_RESOURCES_H_
#define ASH_WEBUI_COMMON_SEA_PEN_RESOURCES_H_

#include "content/public/browser/web_ui_data_source.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::common {

void AddSeaPenStrings(content::WebUIDataSource* source);

}  // namespace ash::common

#endif  // ASH_WEBUI_COMMON_SEA_PEN_RESOURCES_H_
