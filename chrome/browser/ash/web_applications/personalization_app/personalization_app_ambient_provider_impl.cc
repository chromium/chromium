// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"

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
