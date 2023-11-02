// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class Widget;
}  // namespace views

namespace aura {
class Window;
}  // namespace aura

namespace arc {

class ArcResizeLockPrefDelegate;

enum class ResizeCompatMode {
  kPhone,
  kTablet,
  kResizable,
};

void ResizeLockToPhone(views::Widget* widget,
                       ArcResizeLockPrefDelegate* pref_delegate);

void ResizeLockToTablet(views::Widget* widget,
                        ArcResizeLockPrefDelegate* pref_delegate);

void EnableResizingWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate);

ResizeCompatMode PredictCurrentMode(const views::Widget* widget);
ResizeCompatMode PredictCurrentMode(const aura::Window* window);

bool ShouldShowSplashScreenDialog(ArcResizeLockPrefDelegate* pref_delegate);

int GetPortraitPhoneSizeWidth();

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
