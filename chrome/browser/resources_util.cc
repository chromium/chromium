// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/grit/theme_resources_map.h"
#include "components/grit/components_scaled_resources_map.h"
#include "ui/resources/grit/ui_resources_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/resources/grit/ui_chromeos_resources_map.h"
#endif

namespace {

// A wrapper class that holds a map between resource strings and resource
// ids.  This is done so we can use base::NoDestructor which takes care of
// thread safety in initializing the map for us.
class ThemeMap {
 public:
  using StringIntMap = base::flat_map<std::string, int>;

  ThemeMap() {
    size_t storage_size =
        kComponentsScaledResourcesSize + kThemeResourcesSize + kUiResourcesSize;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    storage_size += kUiChromeosResourcesSize;
#endif

    // Construct in one-shot from a moved vector.
    std::vector<StringIntMap::value_type> storage;
    storage.reserve(storage_size);

    for (size_t i = 0; i < kComponentsScaledResourcesSize; ++i) {
      storage.emplace_back(kComponentsScaledResources[i].path,
                           kComponentsScaledResources[i].id);
    }
    for (size_t i = 0; i < kThemeResourcesSize; ++i) {
      storage.emplace_back(kThemeResources[i].path, kThemeResources[i].id);
    }
    for (size_t i = 0; i < kUiResourcesSize; ++i) {
      storage.emplace_back(kUiResources[i].path, kUiResources[i].id);
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    for (size_t i = 0; i < kUiChromeosResourcesSize; ++i) {
      storage.emplace_back(kUiChromeosResources[i].path,
                           kUiChromeosResources[i].id);
    }
#endif

    id_map_ = StringIntMap(std::move(storage));
  }

  int GetId(const std::string& resource_name) const {
    auto it = id_map_.find(resource_name);
    return it != id_map_.end() ? it->second : -1;
  }

 private:
  StringIntMap id_map_;
};

ThemeMap& GetThemeIdsMap() {
  static base::NoDestructor<ThemeMap> s;
  return *s;
}

}  // namespace

int ResourcesUtil::GetThemeResourceId(const std::string& resource_name) {
  return GetThemeIdsMap().GetId(resource_name);
}
