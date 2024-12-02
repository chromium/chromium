// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/settings.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

namespace {
// The following values are calculated from the required percentages, 4% for
// fill opacity and 6% for outline opacity.
constexpr int kDefaultFillOpacity = 0xb;
constexpr int kDefaultOutlineOpacity = 0x11;
constexpr SkColor kFillColor =
    SkColorSetARGB(kDefaultFillOpacity, 0x00, 0x00, 0x00);
constexpr SkColor kOutlineColor =
    SkColorSetARGB(kDefaultOutlineOpacity, 0xff, 0xff, 0xff);

// Command line switches that allow users to set fill and outline opacity values
// as a percentage between 1 and 99, inclusive.
constexpr char kWatermarkFillOpacityPercentFlag[] = "watermark-fill-opacity";
constexpr char kWatermarkOutlineOpacityPercentFlag[] =
    "watermark-outline-opacity";

int GetClampedOpacity(int default_value, const char* cmd_opacity_percent_flag) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(cmd_opacity_percent_flag) &&
      (chrome::GetChannel() != version_info::Channel::STABLE ||
       chrome::GetChannel() != version_info::Channel::BETA)) {
    int opacity = default_value;
    if (base::StringToInt(cmd->GetSwitchValueASCII(cmd_opacity_percent_flag),
                          &opacity)) {
      return std::clamp(opacity, 1, 99) * 256 / 100;
    }
  }
  return default_value;
}

}  // namespace

namespace enterprise_watermark {

SkColor GetFillColor() {
  return SkColorSetA(
      kFillColor,
      GetClampedOpacity(kDefaultFillOpacity, kWatermarkFillOpacityPercentFlag));
}

SkColor GetOutlineColor() {
  return SkColorSetA(kOutlineColor,
                     GetClampedOpacity(kDefaultOutlineOpacity,
                                       kWatermarkOutlineOpacityPercentFlag));
}

}  // namespace enterprise_watermark
