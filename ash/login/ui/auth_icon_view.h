// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_ICON_VIEW_H_
#define ASH_LOGIN_UI_AUTH_ICON_VIEW_H_

#include "ash/login/ui/animated_rounded_image_view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// An icon with a built-in progress bar functionality and animation support used
// to show auth factors (e.g. Fingerprint, Smart Lock) in the
// LoginAuthFactorsView.
// TODO(crbug.com/1252880): Add progress animation.
class AuthIconView : public AnimatedRoundedImageView {
 public:
  AuthIconView();
  AuthIconView(AuthIconView&) = delete;
  AuthIconView& operator=(AuthIconView&) = delete;
  ~AuthIconView() override;

  // Show a static icon.
  void SetIcon(const gfx::VectorIcon& icon);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_ICON_VIEW_H_
