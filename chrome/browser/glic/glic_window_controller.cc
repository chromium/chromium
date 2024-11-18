// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

GlicWindowController* GlicWindowController::GetOrCreateGlicWindowController(
    Profile* profile) {
  return nullptr;
}

void GlicWindowController::Show() {}

void GlicWindowController::Close() {}

GlicWindowController::~GlicWindowController() = default;
