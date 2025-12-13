// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONFIG_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONFIG_H_

#include "base/time/time.h"

namespace glic {

// Helper class for deteriming when/how to reset the panel position.
class GlicWindowConfig {
 public:
  GlicWindowConfig() = default;
  GlicWindowConfig(const GlicWindowConfig&) = delete;
  GlicWindowConfig& operator=(const GlicWindowConfig&) = delete;

  // True if conditions are met to reset the panel location when opening.
  bool ShouldResetOnOpen() const;

  // True if the panel location should be reset on Chrome start.
  bool ShouldResetOnStart() const;

  // True if the panel location should not be saved until a manual drag.
  bool ShouldSetPostionOnDrag() const;

  // True if conditions are met to reset the panel location based on session
  // timeout.
  bool ShouldResetOnNewSession() const;

  // True if the panel size should reset when opening
  bool ShouldResetSizeAndLocationOnShow() const;

  void SetLastOpenTime();
  void SetLastCloseTime();

 private:
  base::TimeDelta GetTopChromeButtonDelay() const;
  base::TimeDelta GetSessionTimeoutDelay() const;
  bool IsButtonClickDelayValid() const;
  base::TimeTicks last_open_time_;
  base::TimeTicks last_close_time_ = base::TimeTicks::Now();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONFIG_H_
