// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"

#include "base/check_op.h"

namespace ash {

namespace {

ArcInputMethodBoundsTracker* g_instance = nullptr;

}  // namespace

// static
ArcInputMethodBoundsTracker* ArcInputMethodBoundsTracker::Get() {
  return g_instance;
}

ArcInputMethodBoundsTracker::ArcInputMethodBoundsTracker() {
  DCHECK(!g_instance);
  g_instance = this;
}

ArcInputMethodBoundsTracker::~ArcInputMethodBoundsTracker() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void ArcInputMethodBoundsTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcInputMethodBoundsTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcInputMethodBoundsTracker::NotifyArcInputMethodBoundsChanged(
    const gfx::Rect& bounds) {
  for (Observer& observer : observers_)
    observer.OnArcInputMethodBoundsChanged(bounds);
}

}  // namespace ash
