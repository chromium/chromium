// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_RESOURCE_LOADER_H_
#define ASH_WEBUI_FILE_MANAGER_RESOURCE_LOADER_H_

#include <stddef.h>

#include "base/containers/span.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/resource_path.h"

namespace ash {
namespace file_manager {

void AddFilesAppResources(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> entries);

}  // namespace file_manager
}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_RESOURCE_LOADER_H_
