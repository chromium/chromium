// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_controller.h"

#include "chrome/common/buildflags.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_window_manager.h"
#endif

GlicController::GlicController() = default;
GlicController::~GlicController() = default;

void GlicController::Show() {
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicWindowManager::GetInstance()->ShowGlicWindowForPinnedProfile();
#endif
}

void GlicController::Hide() {
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicWindowManager::GetInstance()->CloseGlicWindow();
#endif
}
