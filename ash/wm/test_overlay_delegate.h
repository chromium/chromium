// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_OVERLAY_DELEGATE_H_
#define ASH_WM_TEST_OVERLAY_DELEGATE_H_

#include "ash/wm/overlay_event_filter.h"

namespace ash {

class TestOverlayDelegate : public OverlayEventFilter::Delegate {
 public:
  TestOverlayDelegate();

  TestOverlayDelegate(const TestOverlayDelegate&) = delete;
  TestOverlayDelegate& operator=(const TestOverlayDelegate&) = delete;

  virtual ~TestOverlayDelegate();

  int GetCancelCountAndReset();

 private:
  // Overridden from OverlayEventFilter::Delegate:
  void Cancel() override;
  bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  aura::Window* GetWindow() override;

  int cancel_count_;
};

}  // namespace ash

#endif  // ASH_WM_TEST_OVERLAY_DELEGATE_H_
