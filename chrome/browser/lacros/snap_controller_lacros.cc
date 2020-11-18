// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/snap_controller_lacros.h"

#include "base/notreached.h"

SnapControllerLacros::SnapControllerLacros() = default;
SnapControllerLacros::~SnapControllerLacros() = default;

bool SnapControllerLacros::CanSnap(aura::Window* window) {
  NOTIMPLEMENTED();
  return false;
}
void SnapControllerLacros::ShowSnapPreview(aura::Window* window,
                                           chromeos::SnapDirection snap) {
  NOTIMPLEMENTED();
}
void SnapControllerLacros::CommitSnap(aura::Window* window,
                                      chromeos::SnapDirection snap) {
  NOTIMPLEMENTED();
}
