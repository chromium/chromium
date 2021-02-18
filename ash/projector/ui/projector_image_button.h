// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_IMAGE_BUTTON_H_
#define ASH_PROJECTOR_UI_PROJECTOR_IMAGE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/projector/ui/projector_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A view that shows a button with a image. This is used for creating icon
// buttons in the Projector bar.
class ASH_EXPORT ProjectorImageButton : public ProjectorButton {
 public:
  ProjectorImageButton(views::Button::PressedCallback callback,
                       const gfx::VectorIcon& icon);
  ProjectorImageButton(const ProjectorImageButton&) = delete;
  ProjectorImageButton& operator=(const ProjectorImageButton&) = delete;
  ~ProjectorImageButton() override = default;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_IMAGE_BUTTON_H_
