// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"

namespace apps {

AppIconReader::AppIconReader() = default;

AppIconReader::~AppIconReader() = default;

void AppIconReader::ReadIcons(const std::string& app_id,
                              int32_t size_hint_in_dip,
                              IconEffects icon_effects,
                              LoadIconCallback callback) {
  // TODO(crbug.com/1380608): Implement the icon reading function.
}

}  // namespace apps
