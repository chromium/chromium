// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_COUNT_ICON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_COUNT_ICON_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_app_icon.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ASH_EXPORT AppCountIcon : public AppIcon {
  METADATA_HEADER(AppCountIcon, AppIcon)

 public:
  explicit AppCountIcon(const int count);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_COUNT_ICON_H_
