// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ANDROID_THEME_RESOURCES_H_
#define CHROME_BROWSER_ANDROID_ANDROID_THEME_RESOURCES_H_

#include "components/resources/android/theme_resources.h"

// LINK_RESOURCE_ID will use an ID defined by grit, so no-op.
#define LINK_RESOURCE_ID(c_id, java_id)
// For DECLARE_RESOURCE_ID, make an entry in an enum.
#define DECLARE_RESOURCE_ID(c_id, java_id) c_id,

enum {
  // Start after components IDs to make sure there are no conflicts.
  ANDROID_CHROME_RESOURCE_ID_NONE = ANDROID_COMPONENTS_RESOURCE_ID_MAX,
#include "chrome/browser/android/resource_id.h"
};

#undef LINK_RESOURCE_ID
#undef DECLARE_RESOURCE_ID

#endif  // CHROME_BROWSER_ANDROID_ANDROID_THEME_RESOURCES_H_
