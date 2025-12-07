// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/app_list/app_collections_constants.h"

#include <map>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

std::vector<ash::AppCollection> GetAppCollections() {
  return {ash::AppCollection::kUnknown,       ash::AppCollection::kEssentials,
          ash::AppCollection::kProductivity,  ash::AppCollection::kCreativity,
          ash::AppCollection::kEntertainment, ash::AppCollection::kUtilities};
}

std::u16string GetAppCollectionName(ash::AppCollection collection) {
  switch (collection) {
    case ash::AppCollection::kEssentials:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_ESSENTIALS_NAME);
    case ash::AppCollection::kProductivity:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_PRODUCTIVITY_NAME);
    case ash::AppCollection::kCreativity:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_CREATIVITY_NAME);
    case ash::AppCollection::kEntertainment:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_ENTERTAINMENT_NAME);
    case ash::AppCollection::kUtilities:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_UTILITIES_NAME);
    case ash::AppCollection::kUnknown:
      return l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_YOUR_APPS_NAME);
    case ash::AppCollection::kOem:
      NOTREACHED();
  }
}

}  // namespace ash
