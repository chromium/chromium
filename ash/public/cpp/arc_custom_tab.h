// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_CUSTOM_TAB_H_
#define ASH_PUBLIC_CPP_ARC_CUSTOM_TAB_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

// ArcCustomTab is responsible to embed an ARC++ custom tab.
class ASH_EXPORT ArcCustomTab {
 public:
  // Creates a new ArcCustomTab instance. Returns null when the arguments are
  // invalid.
  static std::unique_ptr<ArcCustomTab> Create(aura::Window* arc_app_window,
                                              int32_t surface_id,
                                              int32_t top_margin);

  ArcCustomTab();
  virtual ~ArcCustomTab();

  virtual void Attach(gfx::NativeView view) = 0;

  // Returns the view against which a view or dialog is positioned and parented
  // in an ArcCustomTab.
  virtual gfx::NativeView GetHostView() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcCustomTab);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_CUSTOM_TAB_H_
