// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/app_list/app_collections_constants.h"

#include <map>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"

namespace ash {

std::vector<ash::AppCollection> GetAppCollections() {
  return {ash::AppCollection::kEssentials, ash::AppCollection::kProductivity,
          ash::AppCollection::kCreativity, ash::AppCollection::kEntertainment,
          ash::AppCollection::kUtilities};
}

std::u16string GetAppCollectionName(ash::AppCollection collection) {
  switch (collection) {
    case ash::AppCollection::kEssentials:
      return u"Essentials";
    case ash::AppCollection::kProductivity:
      return u"Productivity";
    case ash::AppCollection::kCreativity:
      return u"Creativity";
    case ash::AppCollection::kEntertainment:
      return u"Entertainment";
    case ash::AppCollection::kUtilities:
      return u"Utilities";
    case ash::AppCollection::kOem:
    case ash::AppCollection::kUnknown:
      NOTREACHED();
      return u"";
  }
}

}  // namespace ash
