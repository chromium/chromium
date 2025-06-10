// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/grit/theme_resources_map.h"
#include "components/grit/components_scaled_resources_map.h"
#include "ui/resources/grit/ui_resources_map.h"

#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
#include "third_party/search_engines_data/search_engines_scaled_resources_map.h"
#endif
#if BUILDFLAG(IS_CHROMEOS)
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
    size_t storage_size = std::size(kComponentsScaledResources) +
                          std::size(kThemeResources) + std::size(kUiResources);
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
    storage_size += std::size(kSearchEnginesScaledResources);
#endif
#if BUILDFLAG(IS_CHROMEOS)
    storage_size += std::size(kUiChromeosResources);
#endif

    // Construct in one-shot from a moved vector.
    std::vector<StringIntMap::value_type> storage;
    storage.reserve(storage_size);

    for (const auto& resource : kComponentsScaledResources) {
      storage.emplace_back(resource.path, resource.id);
    }
    for (const auto& resource : kThemeResources) {
      storage.emplace_back(resource.path, resource.id);
    }
    for (const auto& resource : kUiResources) {
      storage.emplace_back(resource.path, resource.id);
    }
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
    for (const auto& resource : kSearchEnginesScaledResources) {
      storage.emplace_back(resource.path, resource.id);
    }
#endif
#if BUILDFLAG(IS_CHROMEOS)
    for (const auto& resource : kUiChromeosResources) {
      storage.emplace_back(resource.path, resource.id);
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
