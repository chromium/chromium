// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VIEW_SHADOW_H_
#define ASH_PUBLIC_CPP_VIEW_SHADOW_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/view_observer.h"

namespace ui {
class Shadow;
}

namespace ash {

// Manages the shadow for a view. This forces |view| to paint to layer if it's
// not.
class ASH_PUBLIC_EXPORT ViewShadow : public views::ViewObserver,
                                     public ui::LayerOwner::Observer {
 public:
  ViewShadow(views::View* view, int elevation);
  ~ViewShadow() override;

  // Update the corner radius of the view along with the shadow.
  void SetRoundedCornerRadius(int corner_radius);

  // ui::LayerOwner::Observer:
  void OnLayerRecreated(ui::Layer* old_layer) override;

  ui::Shadow* shadow() { return shadow_.get(); }

 private:
  // views::ViewObserver:
  void OnLayerTargetBoundsChanged(views::View* view) override;
  void OnViewIsDeleting(views::View* view) override;

  views::View* view_;
  std::unique_ptr<ui::Shadow> shadow_;

  DISALLOW_COPY_AND_ASSIGN(ViewShadow);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VIEW_SHADOW_H_
