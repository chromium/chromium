// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_

#include "ash/public/cpp/arc_compat_mode_util.h"

namespace views {
class Widget;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

void ResizeLockToPhone(views::Widget* widget,
                       ArcResizeLockPrefDelegate* pref_delegate);

void ResizeLockToTablet(views::Widget* widget,
                        ArcResizeLockPrefDelegate* pref_delegate);

void EnableResizingWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate);

bool ShouldShowSplashScreenDialog(ArcResizeLockPrefDelegate* pref_delegate);

int GetUnresizableSnappedWidth(aura::Window* window);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
