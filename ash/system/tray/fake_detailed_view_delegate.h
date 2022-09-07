// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_FAKE_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_TRAY_FAKE_DETAILED_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/detailed_view_delegate.h"

namespace ash {

// Fake of DetailedViewDelegate implementation.
class ASH_EXPORT FakeDetailedViewDelegate : public DetailedViewDelegate {
 public:
  FakeDetailedViewDelegate();
  FakeDetailedViewDelegate(const FakeDetailedViewDelegate&) = delete;
  FakeDetailedViewDelegate& operator=(const FakeDetailedViewDelegate&) = delete;
  ~FakeDetailedViewDelegate() override;

  size_t close_bubble_call_count() const { return close_bubble_call_count_; }

 private:
  // DetailedViewDelegate:
  void CloseBubble() override;

  size_t close_bubble_call_count_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_FAKE_DETAILED_VIEW_DELEGATE_H_