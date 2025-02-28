// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_
#define ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class OnTaskPodController;

// OnTaskPodView contains the shortcut buttons that are part of the OnTask pod.
// The OnTask pod is meant to supplement OnTask UX with convenience features
// like page navigation, tab reloads, tab strip pinning in locked mode, etc.
class ASH_EXPORT OnTaskPodView : public views::BoxLayoutView {
  METADATA_HEADER(OnTaskPodView, views::BoxLayoutView)

 public:
  explicit OnTaskPodView(OnTaskPodController* pod_controller);
  OnTaskPodView(const OnTaskPodView&) = delete;
  OnTaskPodView& operator=(const OnTaskPodView) = delete;
  ~OnTaskPodView() override;

  // Test element accessors:
  IconButton* reload_tab_button_for_testing() { return reload_tab_button_; }
  IconButton* snap_pod_button_for_testing() { return snap_pod_button_; }

 private:
  // Adds shortcut buttons to the OnTask pod view.
  void AddShortcutButtons();

  // Toggles the snap location for the OnTask pod.
  void ToggleSnapLocation();

  // Pointer to the pod controller that outlives the `OnTaskPodView`.
  const raw_ptr<OnTaskPodController> pod_controller_;

  // Pointers to components hosted by the OnTask pod view.
  raw_ptr<IconButton> snap_pod_button_;
  raw_ptr<views::Separator> left_separator_;
  raw_ptr<IconButton> reload_tab_button_;

  base::WeakPtrFactory<OnTaskPodView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_
