// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/fake_personalization_app_ambient_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "content/public/browser/web_ui.h"

namespace ash::personalization_app {

FakePersonalizationAppAmbientProvider::FakePersonalizationAppAmbientProvider(
    content::WebUI* web_ui) {}

FakePersonalizationAppAmbientProvider::
    ~FakePersonalizationAppAmbientProvider() = default;

void FakePersonalizationAppAmbientProvider::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_receiver_.reset();
  ambient_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppAmbientProvider::IsAmbientModeEnabled(
    IsAmbientModeEnabledCallback callback) {
  std::move(callback).Run(std::move(true));
}

void FakePersonalizationAppAmbientProvider::ShouldShowTimeOfDayBanner(
    ShouldShowTimeOfDayBannerCallback callback) {
  std::move(callback).Run(features::IsTimeOfDayScreenSaverEnabled());
}

void FakePersonalizationAppAmbientProvider::
    IsGeolocationEnabledForSystemServices(
        IsGeolocationEnabledForSystemServicesCallback callback) {
  std::move(callback).Run(geolocation_enabled_for_system);
}

void FakePersonalizationAppAmbientProvider::IsGeolocationUserModifiable(
    IsGeolocationUserModifiableCallback callback) {
  std::move(callback).Run(is_geolocation_user_modifiable);
}

void FakePersonalizationAppAmbientProvider::
    EnableGeolocationForSystemServices() {
  geolocation_enabled_for_system = true;
}

}  // namespace ash::personalization_app
