// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_config.h"

#include "base/time/time.h"
#include "chrome/common/chrome_features.h"

namespace glic {

bool GlicWindowConfig::ShouldResetOnOpen() const {
  return base::FeatureList::IsEnabled(
             features::kGlicPanelResetTopChromeButton) &&
         IsButtonClickDelayValid();
}

bool GlicWindowConfig::ShouldResetOnStart() const {
  return base::FeatureList::IsEnabled(features::kGlicPanelResetOnStart);
}

bool GlicWindowConfig::ShouldSetPostionOnDrag() const {
  return base::FeatureList::IsEnabled(features::kGlicPanelSetPositionOnDrag);
}

bool GlicWindowConfig::ShouldResetOnNewSession() const {
  if (!base::FeatureList::IsEnabled(
          features::kGlicPanelResetOnSessionTimeout)) {
    return false;
  }
  return (base::TimeTicks::Now() - last_close_time_) > GetSessionTimeoutDelay();
}

bool GlicWindowConfig::ShouldResetSizeAndLocationOnShow() const {
  return base::FeatureList::IsEnabled(
      features::kGlicPanelResetSizeAndLocationOnOpen);
}

void GlicWindowConfig::SetLastOpenTime() {
  last_open_time_ = base::TimeTicks::Now();
}

void GlicWindowConfig::SetLastCloseTime() {
  last_close_time_ = base::TimeTicks::Now();
}

base::TimeDelta GlicWindowConfig::GetTopChromeButtonDelay() const {
  return base::Milliseconds(
      features::kGlicPanelResetTopChromeButtonDelayMs.Get());
}

base::TimeDelta GlicWindowConfig::GetSessionTimeoutDelay() const {
  return base::Hours(features::kGlicPanelResetOnSessionTimeoutDelayH.Get());
}

bool GlicWindowConfig::IsButtonClickDelayValid() const {
  return (base::TimeTicks::Now() - last_open_time_) < GetTopChromeButtonDelay();
}

}  // namespace glic
