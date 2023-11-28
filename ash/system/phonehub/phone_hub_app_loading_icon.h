// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_app_icon.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/animation_abort_handle.h"

namespace ash {

class ASH_EXPORT AppLoadingIcon : public AppIcon {
  METADATA_HEADER(AppLoadingIcon, AppIcon)

 public:
  explicit AppLoadingIcon(int size);
  AppLoadingIcon(const AppLoadingIcon&) = delete;
  AppLoadingIcon& operator=(const AppLoadingIcon&) = delete;
  ~AppLoadingIcon() override;

  void StartLoadingAnimation(std::optional<base::TimeDelta> initial_delay);
  void StopLoadingAnimation();

 private:
  std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle_;
  base::OneShotTimer animation_initial_delay_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_LOADING_ICON_H_
