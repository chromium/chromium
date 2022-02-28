// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

AmbientContainerView::AmbientContainerView(
    AmbientViewDelegate* delegate,
    std::unique_ptr<AmbientAnimationStaticResources>
        animation_static_resources) {
  DCHECK(delegate);
  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
  SetID(AmbientViewID::kAmbientContainerView);
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  // Updates focus behavior to receive key press events.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  if (animation_static_resources) {
    AddChildView(std::make_unique<AmbientAnimationView>(
        delegate, std::move(animation_static_resources)));
  } else {
    AddChildView(std::make_unique<PhotoView>(delegate));
  }
}

AmbientContainerView::~AmbientContainerView() = default;

BEGIN_METADATA(AmbientContainerView, views::View)
END_METADATA

}  // namespace ash
