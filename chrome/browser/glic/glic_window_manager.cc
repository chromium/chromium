// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_manager.h"

#include "chrome/browser/glic/glic_keyed_service_factory.h"

GlicWindowManager* GlicWindowManager::GetInstance() {
  return base::Singleton<GlicWindowManager>::get();
}

void GlicWindowManager::ShowGlicWindowForProfile(Profile* profile) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  // If there was already a controller, close the existing window before
  // creating the next one.
  if (glic_window_controller_ &&
      glic_window_controller_.get() != service->window_controller()) {
    CloseGlicWindow();
  }

  service->LaunchUI();
  glic_window_controller_ = service->window_controller()->GetWeakPtr();
}

void GlicWindowManager::CloseGlicWindow() {
  if (glic_window_controller_) {
    glic_window_controller_->Close();
    glic_window_controller_.reset();
  }
}

GlicWindowManager::GlicWindowManager() = default;

GlicWindowManager::~GlicWindowManager() = default;
