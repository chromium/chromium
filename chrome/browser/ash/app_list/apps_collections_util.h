// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APPS_COLLECTIONS_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_APPS_COLLECTIONS_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/common/extension_id.h"

namespace apps_util {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A testing app id that belongs to AppCollection::kEssentials by definition.
extern const char kTestAppIdWithCollection[];

// Obtain the AppCollection where the app belongs into.
ash::AppCollection GetCollectionIdForAppId(const std::string& app_id);

// Obtain the modified default ordinals for the AppsCollecrtions experimental
// arm.
bool GetModifiedOrdinals(const extensions::ExtensionId& extension_id,
                         syncer::StringOrdinal* app_launch_ordinal);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps_util

#endif  // CHROME_BROWSER_ASH_APP_LIST_APPS_COLLECTIONS_UTIL_H_
