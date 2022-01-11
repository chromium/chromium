// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_info.h"
#include "content/public/browser/web_ui.h"

PersonalizationAppUserProviderImpl::PersonalizationAppUserProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

PersonalizationAppUserProviderImpl::~PersonalizationAppUserProviderImpl() =
    default;

void PersonalizationAppUserProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
        receiver) {
  user_receiver_.reset();
  user_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppUserProviderImpl::GetUserInfo(
    GetUserInfoCallback callback) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  std::move(callback).Run(ash::personalization_app::UserDisplayInfo(*user));
}
