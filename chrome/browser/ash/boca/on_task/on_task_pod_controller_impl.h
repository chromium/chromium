// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

class Browser;

namespace ash {

// OnTask pod controller implementation for the `OnTaskPodView`. This controller
// implementation also owns the widget that hosts the pod component view.
class OnTaskPodControllerImpl : public OnTaskPodController {
 public:
  explicit OnTaskPodControllerImpl(Browser* browser);
  OnTaskPodControllerImpl(const OnTaskPodControllerImpl&) = delete;
  OnTaskPodControllerImpl& operator=(const OnTaskPodControllerImpl) = delete;
  ~OnTaskPodControllerImpl() override;

  // OnTaskPodController:
  void ReloadCurrentPage() override;

  // Component accessors used for testing purposes.
  views::Widget* GetPodWidgetForTesting();

 private:
  // Calculates the OnTask pod widget bounds based on the snap location and
  // the parent window frame header height.
  const gfx::Rect CalculateWidgetBounds();

  // Weak pointer for the Boca app instance that is being interacted with.
  const base::WeakPtr<Browser> browser_;

  // Pod widget that contains the `OnTaskPodView`.
  std::unique_ptr<views::Widget> pod_widget_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_
