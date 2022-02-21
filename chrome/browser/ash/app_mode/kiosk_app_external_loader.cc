// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_external_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"

namespace ash {

KioskAppExternalLoader::KioskAppExternalLoader(AppClass app_class)
    : app_class_(app_class) {}

KioskAppExternalLoader::~KioskAppExternalLoader() {
  if (state_ != State::kInitial)
    SetPrefsChangedHandler(InstallDataChangeCallback());
}

void KioskAppExternalLoader::StartLoading() {
  state_ = State::kLoading;

  SetPrefsChangedHandler(base::BindRepeating(&KioskAppExternalLoader::SendPrefs,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void KioskAppExternalLoader::SetPrefsChangedHandler(
    InstallDataChangeCallback handler) {
  switch (app_class_) {
    case AppClass::kPrimary:
      KioskAppManager::Get()->SetPrimaryAppLoaderPrefsChangedHandler(
          std::move(handler));
      break;
    case AppClass::kSecondary:
      KioskAppManager::Get()->SetSecondaryAppsLoaderPrefsChangedHandler(
          std::move(handler));
      break;
  }
}

void KioskAppExternalLoader::SendPrefs(
    std::unique_ptr<base::DictionaryValue> prefs) {
  if (!prefs)
    return;

  const bool initial_load = state_ == State::kLoading;
  state_ = State::kLoaded;

  if (initial_load) {
    LoadFinished(std::move(prefs));
  } else {
    OnUpdated(std::move(prefs));
  }
}

}  // namespace ash
