// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOCK_SCREEN_WIDGET_FACTORY_H_
#define ASH_PUBLIC_CPP_LOCK_SCREEN_WIDGET_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace views {
class View;
class Widget;
}

namespace ash {

// Creates a widget shown on the lock-screen. The widget is configured such
// that the caller owns it (InitParams::WIDGET_OWNS_NATIVE_WIDGET). |parent|
// should only be supplied if called from ash, otherwise use null to get the
// right container.
ASH_PUBLIC_EXPORT std::unique_ptr<views::Widget> CreateLockScreenWidget(
    aura::Window* parent,
    std::unique_ptr<views::View> contents_view);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOCK_SCREEN_WIDGET_FACTORY_H_
