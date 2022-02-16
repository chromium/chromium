// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_external_loader.h"

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
    SetPrefsChangedHandler(base::RepeatingClosure());
}

void KioskAppExternalLoader::StartLoading() {
  if (state_ != State::kInitial) {
    SendPrefsIfAvailable();
    return;
  }

  state_ = State::kLoading;

  SetPrefsChangedHandler(
      base::BindRepeating(&KioskAppExternalLoader::SendPrefsIfAvailable,
                          weak_ptr_factory_.GetWeakPtr()));

  SendPrefsIfAvailable();
}

std::unique_ptr<base::DictionaryValue> KioskAppExternalLoader::GetAppsPrefs() {
  switch (app_class_) {
    case AppClass::kPrimary:
      return KioskAppManager::Get()->GetPrimaryAppLoaderPrefs();
    case AppClass::kSecondary:
      return KioskAppManager::Get()->GetSecondaryAppsLoaderPrefs();
  }
  return nullptr;
}

void KioskAppExternalLoader::SetPrefsChangedHandler(
    base::RepeatingClosure handler) {
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

void KioskAppExternalLoader::SendPrefsIfAvailable() {
  std::unique_ptr<base::DictionaryValue> prefs = GetAppsPrefs();
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
