// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class ActionButtonView;

// A view that displays a row of action buttons near the capture region.
class ASH_EXPORT ActionButtonContainerView : public views::View {
  METADATA_HEADER(ActionButtonContainerView, views::View)

 public:
  ActionButtonContainerView();
  ActionButtonContainerView(const ActionButtonContainerView&) = delete;
  ActionButtonContainerView& operator=(const ActionButtonContainerView&) =
      delete;
  ~ActionButtonContainerView() override;

  // Adds an action button to the container. Returns a pointer to the added
  // button.
  // TODO(crbug.com/372740410): Determine behavior when we add a button with the
  // exact same rank (type and priority) as an existing valid button.
  ActionButtonView* AddActionButton(views::Button::PressedCallback callback,
                                    std::u16string text,
                                    const gfx::VectorIcon* icon,
                                    ActionButtonRank rank,
                                    ActionButtonViewID id);
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
