// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"

#include <utility>
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

PersonalizationAppAmbientProviderImpl::PersonalizationAppAmbientProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

PersonalizationAppAmbientProviderImpl::
    ~PersonalizationAppAmbientProviderImpl() = default;

void PersonalizationAppAmbientProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_receiver_.reset();
  ambient_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppAmbientProviderImpl::IsAmbientModeEnabled(
    IsAmbientModeEnabledCallback callback) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  std::move(callback).Run(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

void PersonalizationAppAmbientProviderImpl::SetAmbientObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
        observer) {
  // May already be bound if user refreshes page.
  ambient_observer_remote_.reset();
  ambient_observer_remote_.Bind(std::move(observer));

  // Call it once to get the current ambient mode.
  PrefService* pref_service = profile_->GetPrefs();
  OnAmbientModeEnabledChanged(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

void PersonalizationAppAmbientProviderImpl::SetAmbientModeEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, enabled);
}

void PersonalizationAppAmbientProviderImpl::OnAmbientModeEnabledChanged(
    bool ambient_mode_enabled) {
  DCHECK(ambient_observer_remote_.is_bound());
  ambient_observer_remote_->OnAmbientModeEnabledChanged(ambient_mode_enabled);
}
