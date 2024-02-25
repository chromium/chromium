// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/intent_picker_info.h"

#include <utility>

namespace apps {

IntentPickerAppInfo::IntentPickerAppInfo(PickerEntryType type,
                                         ui::ImageModel icon_model,
                                         std::string launch_name,
                                         std::string display_name)
    : type(std::move(type)),
      icon_model(std::move(icon_model)),
      launch_name(std::move(launch_name)),
      display_name(std::move(display_name)) {}

IntentPickerAppInfo::IntentPickerAppInfo(const IntentPickerAppInfo&) = default;

IntentPickerAppInfo::IntentPickerAppInfo(IntentPickerAppInfo&&) = default;

IntentPickerAppInfo& IntentPickerAppInfo::operator=(
    const IntentPickerAppInfo&) = default;

IntentPickerAppInfo& IntentPickerAppInfo::operator=(IntentPickerAppInfo&&) =
    default;

}  // namespace apps
