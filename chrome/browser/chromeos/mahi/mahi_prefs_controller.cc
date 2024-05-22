// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

namespace mahi {

namespace {

MahiPrefsController* g_mahi_prefs_controller = nullptr;

}  // namespace

// static
MahiPrefsController* MahiPrefsController::Get() {
  return g_mahi_prefs_controller;
}

MahiPrefsController::MahiPrefsController() {
  DCHECK(!g_mahi_prefs_controller);
  g_mahi_prefs_controller = this;
}

MahiPrefsController::~MahiPrefsController() {
  DCHECK_EQ(g_mahi_prefs_controller, this);
  g_mahi_prefs_controller = nullptr;
}

bool MahiPrefsController::GetMahiEnabled() {
  return MahiWebContentsManager::Get()->GetPrefValue();
}

}  // namespace mahi
