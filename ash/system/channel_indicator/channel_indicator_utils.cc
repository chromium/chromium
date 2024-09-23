// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_utils.h"

#include <string>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/color_palette.h"

namespace ash::channel_indicator_utils {

bool IsDisplayableChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      return true;
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return false;
  }
}

int GetChannelNameStringResourceID(version_info::Channel channel,
                                   bool append_channel) {
  switch (channel) {
    case version_info::Channel::BETA:
      return append_channel ? IDS_ASH_STATUS_TRAY_CHANNEL_BETA_CHANNEL
                            : IDS_ASH_STATUS_TRAY_CHANNEL_BETA;
    case version_info::Channel::DEV:
      return append_channel ? IDS_ASH_STATUS_TRAY_CHANNEL_DEV_CHANNEL
                            : IDS_ASH_STATUS_TRAY_CHANNEL_DEV;
    case version_info::Channel::CANARY:
      return append_channel ? IDS_ASH_STATUS_TRAY_CHANNEL_CANARY_CHANNEL
                            : IDS_ASH_STATUS_TRAY_CHANNEL_CANARY;
    // Handle STABLE/UNKNOWN here to satisfy the compiler without using
    // "default," but the DCHECK() above will bark if that value is ever
    // actually passed in.
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return -1;
  }
}

SkColor GetFgColor(version_info::Channel channel) {
  const bool is_dark_mode_enabled =
      DarkLightModeController::Get()->IsDarkModeEnabled();
  switch (channel) {
    case version_info::Channel::BETA:
      return is_dark_mode_enabled ? gfx::kGoogleBlue200 : gfx::kGoogleBlue900;
    case version_info::Channel::DEV:
      return is_dark_mode_enabled ? gfx::kGoogleGreen200 : gfx::kGoogleGreen900;
    case version_info::Channel::CANARY:
      return is_dark_mode_enabled ? gfx::kGoogleYellow200 : gfx::kGoogleGrey900;
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return SkColorSetRGB(0x00, 0x00, 0x00);
  }
}

ui::ColorId GetFgColorJelly(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::BETA:
      return cros_tokens::kCrosSysOnProgressContainer;
    case version_info::Channel::DEV:
      return cros_tokens::kCrosSysOnPositiveContainer;
    case version_info::Channel::CANARY:
      return cros_tokens::kCrosSysOnWarningContainer;
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return ui::ColorId();
  }
}

SkColor GetBgColor(version_info::Channel channel) {
  const bool is_dark_mode_enabled =
      DarkLightModeController::Get()->IsDarkModeEnabled();
  switch (channel) {
    case version_info::Channel::BETA:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleBlue300, 0x55)
                                  : gfx::kGoogleBlue200;
    case version_info::Channel::DEV:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleGreen300, 0x55)
                                  : gfx::kGoogleGreen200;
    case version_info::Channel::CANARY:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleYellow300, 0x55)
                                  : gfx::kGoogleYellow200;
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return SkColorSetRGB(0x00, 0x00, 0x00);
  }
}

ui::ColorId GetBgColorJelly(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::BETA:
      return cros_tokens::kCrosSysProgressContainer;
    case version_info::Channel::DEV:
      return cros_tokens::kCrosSysPositiveContainer;
    case version_info::Channel::CANARY:
      return cros_tokens::kCrosSysWarningContainer;
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
      return ui::ColorId();
  }
}

std::u16string GetFullReleaseTrackString(version_info::Channel channel) {
  if (!IsDisplayableChannel(channel))
    return std::u16string();

  return base::StrCat(
      {l10n_util::GetStringUTF16(
           channel_indicator_utils::GetChannelNameStringResourceID(channel,
                                                                   false)),
       u" ",
       base::UTF8ToUTF16(Shell::Get()->shell_delegate()->GetVersionString())});
}

const gfx::VectorIcon& GetVectorIcon(version_info::Channel channel) {
  DCHECK(IsDisplayableChannel(channel));
  switch (channel) {
    case version_info::Channel::BETA:
      return kChannelBetaIcon;
    case version_info::Channel::DEV:
      return kChannelDevIcon;
    case version_info::Channel::CANARY:
      return kChannelCanaryIcon;
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::STABLE:
      NOTREACHED();
  }
}

}  // namespace ash::channel_indicator_utils
