// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_manager.h"

GlicWindowManager* GlicWindowManager::GetInstance() {
  return base::Singleton<GlicWindowManager>::get();
}

void GlicWindowManager::ShowGlicWindowForProfile(Profile* profile) {
  // If there was already a controller, close the existing window before
  // creating the next one.
  if (glic_window_controller_) {
    CloseGlicWindow();
  }

  auto* controller =
      GlicWindowController::GetOrCreateGlicWindowController(profile);

  if (controller) {
    glic_window_controller_ = controller->GetWeakPtr();
    ;
    glic_window_controller_->Show();
  }
}

void GlicWindowManager::CloseGlicWindow() {
  if (glic_window_controller_) {
    glic_window_controller_->Close();
    glic_window_controller_.reset();
  }
}

GlicWindowManager::GlicWindowManager() = default;

GlicWindowManager::~GlicWindowManager() = default;
