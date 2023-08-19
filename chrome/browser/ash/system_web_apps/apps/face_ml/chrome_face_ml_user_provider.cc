// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/face_ml/chrome_face_ml_user_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

ChromeFaceMLUserProvider::ChromeFaceMLUserProvider(content::WebUI* web_ui)
    : web_ui_(*web_ui) {}

mojom::face_ml_app::UserInformation
ChromeFaceMLUserProvider::GetCurrentUserInformation() {
  Profile* profile = Profile::FromWebUI(&*web_ui_);
  return mojom::face_ml_app::UserInformation(
      /*user_name=*/profile->GetProfileUserName(),
      /*is_signed_in=*/false);
}
}  // namespace ash
