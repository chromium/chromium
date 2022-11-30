// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_VIEW_DRAWN_WAITER_H_
#define ASH_TEST_VIEW_DRAWN_WAITER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace base {
class RunLoop;
}

namespace ash {

// Helper that allows waiting for a view to be drawn. See `Wait()` below. An
// instance can be used more than once (i.e. to wait on a different view).
class ViewDrawnWaiter : public views::ViewObserver {
 public:
  ViewDrawnWaiter();
  ViewDrawnWaiter(const ViewDrawnWaiter&) = delete;
  ViewDrawnWaiter& operator=(const ViewDrawnWaiter&) = delete;
  ~ViewDrawnWaiter() override;

  // Waits for `view` to be drawn (implying visible) and have non-zero size
  // (implying layout is complete).
  void Wait(views::View* view);

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override;
  void OnViewBoundsChanged(views::View* view) override;
  void OnViewIsDeleting(views::View* view) override;

  std::unique_ptr<base::RunLoop> wait_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_TEST_VIEW_DRAWN_WAITER_H_
