// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_

#include "base/memory/raw_ptr.h"

namespace ash {
class OverviewSession;

class OverviewFocusCycler {
 public:
  explicit OverviewFocusCycler(OverviewSession* overview_session);
  OverviewFocusCycler(const OverviewFocusCycler&) = delete;
  OverviewFocusCycler& operator=(const OverviewFocusCycler&) = delete;
  ~OverviewFocusCycler();

  // Moves the focus ring to the next traversable view.
  void MoveFocus(bool reverse);

 private:
  // The overview session which owns this object. Guaranteed to be non-null for
  // the lifetime of `this`.
  const raw_ptr<OverviewSession> overview_session_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_
