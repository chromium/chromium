// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_UTILS_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_UTILS_H_

#include <string>

#include "ash/ash_export.h"
#include "components/version_info/channel.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash::channel_indicator_utils {

// Returns 'true' if `channel` is a release track name we want to show the user.
ASH_EXPORT bool IsDisplayableChannel(version_info::Channel channel);

// Returns a string resource ID for the release track `channel`. If
// `append_channel` is 'true' then the resource ID returned is for a string that
// has "channel" at the end e.g. "Beta Channel" instead of just "Beta". If
// `channel` is `STABLE` or `UNKNOWN` this function will return -1.
ASH_EXPORT int GetChannelNameStringResourceID(version_info::Channel channel,
                                              bool append_channel);

// Returns the foreground UI color for release track `channel`. If `channel` is
// one of the displayable values then the expected `SkColor` is returned, a
// value of SkColorSetRGB(0x00, 0x00, 0x00) otherwise.
ASH_EXPORT SkColor GetFgColor(version_info::Channel channel);

// For Jelly: returns the foreground `ui::ColorId` for release track `channel`.
// If `channel` is one of the displayable values then the expected `ColorId` is
// returned, a value of `ui::ColorId()` otherwise.
ASH_EXPORT ui::ColorId GetFgColorJelly(version_info::Channel channel);

// Returns the background UI color for release track `channel`. If `channel` is
// one of the displayable values then the expected `SkColor` is returned, a
// value of SkColorSetRGB(0x00, 0x00, 0x00) otherwise.
ASH_EXPORT SkColor GetBgColor(version_info::Channel channel);

// For Jelly returns the background `ui::ColorId` for release track `channel`.
// If `channel` is one of the displayable values then the expected `ColorId` is
// returned, a value of `ui::ColorId()` otherwise.
ASH_EXPORT ui::ColorId GetBgColorJelly(version_info::Channel channel);

// Returns the text for the version button text, for release track `channel`
// e.g. "Beta 105.0.5167.0". If `channel` is not one of the displayable values,
// the function will return an empty std::u16string.
ASH_EXPORT std::u16string GetFullReleaseTrackString(
    version_info::Channel channel);

// Returns a reference to the pre-constructed vector icon for release track
// `channel`. The function performs a DCHECK() that `channel` is one of the
// displayable values, but if it isn't then the icon for `CANARY` is returned.
const gfx::VectorIcon& GetVectorIcon(version_info::Channel channel);

}  // namespace ash::channel_indicator_utils

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_UTILS_H_
