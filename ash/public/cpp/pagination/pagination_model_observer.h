// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PAGINATION_PAGINATION_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_PAGINATION_PAGINATION_MODEL_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ASH_PUBLIC_EXPORT PaginationModelObserver {
 public:
  // Invoked when the total number of pages is changed.
  virtual void TotalPagesChanged(int previous_page_count, int new_page_count) {}

  // Invoked when the selected page index is changed.
  virtual void SelectedPageChanged(int old_selected, int new_selected) {}

  // Invoked right before a transition starts.
  virtual void TransitionStarting() {}

  // Invoked right after a transition started.
  virtual void TransitionStarted() {}

  // Invoked when the transition data is changed.
  virtual void TransitionChanged() {}

  // Invoked when a transition ends.
  virtual void TransitionEnded() {}

  // Invoked when a grid scroll starts.
  virtual void ScrollStarted() {}

  // Invoked when a grid scroll ends.
  virtual void ScrollEnded() {}

 protected:
  virtual ~PaginationModelObserver() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PAGINATION_PAGINATION_MODEL_OBSERVER_H_
