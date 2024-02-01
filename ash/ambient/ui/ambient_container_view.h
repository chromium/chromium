// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AmbientUiSettings;

namespace ambient {
class AmbientOrientationMetricsRecorder;
}  // namespace ambient

// Container view to display all Ambient Mode related views, i.e. photo frame,
// weather info.
class ASH_EXPORT AmbientContainerView : public views::View {
  METADATA_HEADER(AmbientContainerView, views::View)

 public:
  // |main_rendering_view| should contain the primary content; it becomes a
  // child of |AmbientContainerView|, and |AmbientContainerView| sets up some
  // parameters in the view hierarchy that are common to all ambient UIs.
  AmbientContainerView(AmbientUiSettings ui_settings,
                       std::unique_ptr<views::View> main_rendering_view);
  ~AmbientContainerView() override;

 private:
  friend class AmbientAshTestBase;

  void InitializeCommonSettings();

  std::unique_ptr<ambient::AmbientOrientationMetricsRecorder>
      orientation_metrics_recorder_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
