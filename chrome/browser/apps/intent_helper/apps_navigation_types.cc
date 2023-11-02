// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"

namespace apps {

IntentPickerAppInfo::IntentPickerAppInfo(PickerEntryType type,
                                         const ui::ImageModel& icon_model,
                                         const std::string& launch_name,
                                         const std::string& display_name)
    : type(type),
      icon_model(icon_model),
      launch_name(launch_name),
      display_name(display_name) {}

IntentPickerAppInfo::IntentPickerAppInfo(IntentPickerAppInfo&& other) = default;

IntentPickerAppInfo& IntentPickerAppInfo::operator=(
    IntentPickerAppInfo&& other) = default;

}  // namespace apps
