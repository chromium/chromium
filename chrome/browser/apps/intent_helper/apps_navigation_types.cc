// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"

namespace apps {

IntentPickerAppInfo::IntentPickerAppInfo(PickerEntryType type,
                                         const gfx::Image& icon,
                                         const std::string& launch_name,
                                         const std::string& display_name)
    : type(type),
      icon(icon),
      launch_name(launch_name),
      display_name(display_name) {}

IntentPickerAppInfo::IntentPickerAppInfo(IntentPickerAppInfo&& other) = default;

IntentPickerAppInfo& IntentPickerAppInfo::operator=(
    IntentPickerAppInfo&& other) = default;

}  // namespace apps
