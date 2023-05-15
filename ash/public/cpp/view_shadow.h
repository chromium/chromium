// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VIEW_SHADOW_H_
#define ASH_PUBLIC_CPP_VIEW_SHADOW_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"
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

  ViewShadow(const ViewShadow&) = delete;
  ViewShadow& operator=(const ViewShadow&) = delete;

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

  raw_ptr<views::View, ExperimentalAsh> view_;
  std::unique_ptr<ui::Shadow> shadow_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VIEW_SHADOW_H_
