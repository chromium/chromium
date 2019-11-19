// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class AmbientAssistantContainerView;
class AmbientController;
class PhotoView;

// Container view for ambient mode.
class ASH_EXPORT AmbientContainerView : public views::WidgetDelegateView {
 public:
  explicit AmbientContainerView(AmbientController* ambient_controller);
  ~AmbientContainerView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void Init();

  AmbientController* ambient_controller_ = nullptr;

  // Owned by view hierarchy.
  PhotoView* photo_view_ = nullptr;
  AmbientAssistantContainerView* ambient_assistant_container_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AmbientContainerView);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
