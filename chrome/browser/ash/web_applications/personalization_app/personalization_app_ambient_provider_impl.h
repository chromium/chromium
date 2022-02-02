// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class PersonalizationAppAmbientProviderImpl
    : public ash::PersonalizationAppAmbientProvider {
 public:
  explicit PersonalizationAppAmbientProviderImpl(content::WebUI* web_ui);

  PersonalizationAppAmbientProviderImpl(
      const PersonalizationAppAmbientProviderImpl&) = delete;
  PersonalizationAppAmbientProviderImpl& operator=(
      const PersonalizationAppAmbientProviderImpl&) = delete;

  ~PersonalizationAppAmbientProviderImpl() override;

  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
          receiver) override;

  // ash::personalization_app::mojom:AmbientProvider:
  void IsAmbientModeEnabled(IsAmbientModeEnabledCallback callback) override;
  void SetAmbientObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
          observer) override;
  void SetAmbientModeEnabled(bool enabled) override;

  // TODO(b/216307771): Will need to add observer for this.
  void OnAmbientModeEnabledChanged(bool ambient_mode_enabled);

 private:
  mojo::Receiver<ash::personalization_app::mojom::AmbientProvider>
      ambient_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_remote_;

  raw_ptr<Profile> const profile_ = nullptr;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_
