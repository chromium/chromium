// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/resource_loader.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"

namespace ash {
namespace file_manager {

void AddFilesAppResources(content::WebUIDataSource* source,
                          const webui::ResourcePath* entries,
                          size_t size) {
  // For Jelly we need to remap some resource dependencies.
  std::map<std::string, int> resource_map;
  if (ash::features::IsJellyEnabled()) {
    for (size_t i = 0; i < size; ++i) {
      const base::FilePath file_path(entries[i].path);
      if (file_path.Extension() == ".css") {
        resource_map[std::string(entries[i].path)] = entries[i].id;
      }
    }
  }
  for (size_t i = 0; i < size; ++i) {
    std::string path(entries[i].path);
    // Only load resources for Files app.
    if (base::StartsWith(path, "file_manager/") &&
        path.find("untrusted_resources/") == std::string::npos) {
      // Files app UI has all paths relative to //ui/file_manager/file_manager/
      // so we remove the leading file_manager/ to match the existing paths.
      base::ReplaceFirstSubstringAfterOffset(&path, 0, "file_manager/", "");
      if (ash::features::IsJellyEnabled()) {
        // Serve CSS files that have the suffix _gm3.css as if they were named
        // without the suffix, serve the content of foo_gm3.css for requests
        // for foo.css.
        const base::FilePath file_path(entries[i].path);
        if (file_path.Extension() == ".css") {
          const std::string gm3_counterpart(
              file_path.RemoveExtension().value() + "_gm3" +
              file_path.Extension());
          auto gm3_resource = resource_map.find(gm3_counterpart);
          if (gm3_resource != resource_map.end()) {
            source->AddResourcePath(path, gm3_resource->second);
            continue;
          }
        }
      }
      source->AddResourcePath(path, entries[i].id);
    }
  }
}

}  // namespace file_manager
}  // namespace ash
