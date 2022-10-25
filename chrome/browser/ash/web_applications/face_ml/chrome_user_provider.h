// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACE_ML_CHROME_USER_PROVIDER_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACE_ML_CHROME_USER_PROVIDER_H_

#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"
#include "ash/webui/face_ml_app_ui/user_provider.h"
#include "base/memory/raw_ref.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
// GetProfileUserNameImplementation of the ParticipantManager interface.
// Provides the FaceML App code with functions that only exist in chrome/.
//
// The ChromeUserProvider provides information of current Chromium user
// from browser context.
class ChromeUserProvider : public UserProvider {
 public:
  explicit ChromeUserProvider(content::WebUI* web_ui);

  ChromeUserProvider(const ChromeUserProvider&) = delete;
  ChromeUserProvider& operator=(const ChromeUserProvider&) = delete;

  mojom::face_ml_app::UserInformation GetCurrentUserInformation() override;

 private:
  const base::raw_ref<content::WebUI> web_ui_;  // Owns |this|.
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACE_ML_CHROME_USER_PROVIDER_H_
