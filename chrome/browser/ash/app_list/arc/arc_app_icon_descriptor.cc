// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_icon_descriptor.h"

#include <math.h>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace {

int GetScalePercent(ui::ResourceScaleFactor scale_factor) {
  return roundf(100.0f * ui::GetScaleForResourceScaleFactor(scale_factor));
}

// Template for the icon name. First part is scale percent and second is
// resource size in dip.
constexpr char kIconNameTemplate[] = "icon_%dp_%d.png";
constexpr char kForegroundIconNameTemplate[] = "foreground_icon_%dp_%d.png";
constexpr char kBackgroundIconNameTemplate[] = "background_icon_%dp_%d.png";

}  // namespace

ArcAppIconDescriptor::ArcAppIconDescriptor(int dip_size,
                                           ui::ResourceScaleFactor scale_factor)
    : dip_size(dip_size), scale_factor(scale_factor) {
  DCHECK_GT(dip_size, 0);
  DCHECK_GT(scale_factor, ui::ResourceScaleFactor::kScaleFactorNone);
  DCHECK_LE(scale_factor, ui::ResourceScaleFactor::k300Percent);
}

int ArcAppIconDescriptor::GetSizeInPixels() const {
  return roundf(dip_size * ui::GetScaleForResourceScaleFactor(scale_factor));
}

std::string ArcAppIconDescriptor::GetName() const {
  return base::StringPrintf(kIconNameTemplate, GetScalePercent(scale_factor),
                            dip_size);
}

std::string ArcAppIconDescriptor::GetForegroundIconName() const {
  return base::StringPrintf(kForegroundIconNameTemplate,
                            GetScalePercent(scale_factor), dip_size);
}

std::string ArcAppIconDescriptor::GetBackgroundIconName() const {
  return base::StringPrintf(kBackgroundIconNameTemplate,
                            GetScalePercent(scale_factor), dip_size);
}

bool ArcAppIconDescriptor::operator==(const ArcAppIconDescriptor& other) const {
  return scale_factor == other.scale_factor && dip_size == other.dip_size;
}

bool ArcAppIconDescriptor::operator!=(const ArcAppIconDescriptor& other) const {
  return !(*this == other);
}

bool ArcAppIconDescriptor::operator<(const ArcAppIconDescriptor& other) const {
  if (dip_size != other.dip_size)
    return dip_size < other.dip_size;
  return static_cast<int>(scale_factor) < static_cast<int>(other.scale_factor);
}
