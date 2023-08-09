// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"

namespace chromeos {

KioskAppExternalLoader::KioskAppExternalLoader(AppClass app_class)
    : app_class_(app_class) {}

KioskAppExternalLoader::~KioskAppExternalLoader() {
  if (state_ != State::kInitial) {
    SetPrefsChangedHandler(
        ChromeKioskExternalLoaderBroker::InstallDataChangeCallback());
  }
}

void KioskAppExternalLoader::StartLoading() {
  state_ = State::kLoading;

  SetPrefsChangedHandler(base::BindRepeating(&KioskAppExternalLoader::SendPrefs,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void KioskAppExternalLoader::SetPrefsChangedHandler(
    ChromeKioskExternalLoaderBroker::InstallDataChangeCallback handler) {
  switch (app_class_) {
    case AppClass::kPrimary:
      ChromeKioskExternalLoaderBroker::Get()
          ->RegisterPrimaryAppInstallDataObserver(std::move(handler));
      break;
    case AppClass::kSecondary:
      ChromeKioskExternalLoaderBroker::Get()
          ->RegisterSecondaryAppInstallDataObserver(std::move(handler));
      break;
  }
}

void KioskAppExternalLoader::SendPrefs(base::Value::Dict prefs) {
  const bool initial_load = state_ == State::kLoading;
  state_ = State::kLoaded;

  if (initial_load) {
    LoadFinished(std::move(prefs));
  } else {
    OnUpdated(std::move(prefs));
  }
}

}  // namespace chromeos
