// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_

#include <string>

#include "ash/components/arc/mojom/compatibility_mode.mojom.h"

namespace arc {

class ArcResizeLockPrefDelegate {
 public:
  virtual ~ArcResizeLockPrefDelegate() = default;

  virtual mojom::ArcResizeLockState GetResizeLockState(
      const std::string& app_id) const = 0;
  virtual void SetResizeLockState(const std::string& app_id,
                                  mojom::ArcResizeLockState state) = 0;
  virtual bool GetResizeLockNeedsConfirmation(const std::string& app_id) = 0;
  virtual void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                              bool is_needed) = 0;
  virtual int GetShowSplashScreenDialogCount() const = 0;
  virtual void SetShowSplashScreenDialogCount(int count) = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_
