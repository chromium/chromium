// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_OOBE_DIALOG_UTIL_H_
#define ASH_PUBLIC_CPP_OOBE_DIALOG_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

// Singleton class that allows ash code to access defines and methods used to
// format UI elements according to the OOBE style.
class ASH_PUBLIC_EXPORT OobeDialogUtil {
 public:
  static OobeDialogUtil& Get();

  OobeDialogUtil();
  virtual ~OobeDialogUtil();

  virtual int GetShadowElevation() = 0;
  virtual int GetCornerRadius() = 0;

  virtual gfx::Size CalculateDialogSize(const gfx::Size& host_size,
                                        int shelf_height,
                                        bool is_horizontal) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_OOBE_DIALOG_UTIL_H_
