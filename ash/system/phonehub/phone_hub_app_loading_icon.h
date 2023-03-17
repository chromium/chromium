// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_app_icon.h"

namespace ash {

class ASH_EXPORT AppLoadingIcon : public AppIcon {
 public:
  explicit AppLoadingIcon(int size);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_
