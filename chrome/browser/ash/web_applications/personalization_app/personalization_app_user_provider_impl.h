// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_user_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

class PersonalizationAppUserProviderImpl
    : public ash::PersonalizationAppUserProvider {
 public:
  explicit PersonalizationAppUserProviderImpl(content::WebUI* web_ui);

  PersonalizationAppUserProviderImpl(
      const PersonalizationAppUserProviderImpl&) = delete;
  PersonalizationAppUserProviderImpl& operator=(
      const PersonalizationAppUserProviderImpl&) = delete;

  ~PersonalizationAppUserProviderImpl() override;

  // PersonalizationAppUserProvider:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
          receiver) override;

  // personalization_app::mojom::UserProvider:
  void GetUserInfo(GetUserInfoCallback callback) override;

 private:
  // Pointer to profile of user that opened personalization SWA. Not owned.
  Profile* const profile_ = nullptr;

  mojo::Receiver<ash::personalization_app::mojom::UserProvider> user_receiver_{
      this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_
