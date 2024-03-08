// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_COLLECTIONS_CONSTANTS_H_
#define ASH_APP_LIST_APP_COLLECTIONS_CONSTANTS_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"

namespace ash {

// Fetch the list of available AppCollection ids that have data within the
// `kPredefinedAppsCollections` list.
ASH_EXPORT std::vector<ash::AppCollection> GetAppCollections();

// Obtain the display name of the `collection`.
ASH_EXPORT std::u16string GetAppCollectionName(ash::AppCollection collection);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_COLLECTIONS_CONSTANTS_H_
