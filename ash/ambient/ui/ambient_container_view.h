// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AmbientAssistantContainerView;
class AmbientViewDelegate;
class PhotoView;

// Container view to display all Ambient Mode related views, i.e. photo frame,
// weather info.
class ASH_EXPORT AmbientContainerView : public views::View {
 public:
  explicit AmbientContainerView(AmbientViewDelegate* delegate);
  ~AmbientContainerView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void AddedToWidget() override;

 private:
  friend class AmbientAshTestBase;
  class HostWidgetEventObserver;

  void Init();

  // Layouts its child views.
  // TODO(meilinw): Use LayoutManagers to lay out children instead of overriding
  // Layout(). See b/163170162.
  void LayoutPhotoView();
  void LayoutAssistantView();

  // Invoked on specific types of events.
  void HandleEvent();

  AmbientViewDelegate* delegate_ = nullptr;

  // Owned by view hierarchy.
  PhotoView* photo_view_ = nullptr;
  AmbientAssistantContainerView* ambient_assistant_container_view_ = nullptr;

  // Observes events from its host widget.
  std::unique_ptr<HostWidgetEventObserver> event_observer_;

  DISALLOW_COPY_AND_ASSIGN(AmbientContainerView);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
