// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/crosapi_pref_observer.h"

#include "base/callback.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

CrosapiPrefObserver::CrosapiPrefObserver(crosapi::mojom::PrefPath path,
                                         PrefChangedCallback callback)
    : callback_(std::move(callback)) {
  auto* lacros_service = chromeos::LacrosChromeServiceImpl::Get();
  if (!lacros_service->IsPrefsAvailable()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->prefs_remote()->AddObserver(
      path, receiver_.BindNewPipeAndPassRemote());
}

CrosapiPrefObserver::~CrosapiPrefObserver() = default;

void CrosapiPrefObserver::OnPrefChanged(base::Value value) {
  callback_.Run(std::move(value));
}
