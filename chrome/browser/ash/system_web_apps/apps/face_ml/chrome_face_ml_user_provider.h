// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FACE_ML_CHROME_FACE_ML_USER_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FACE_ML_CHROME_FACE_ML_USER_PROVIDER_H_

#include "ash/webui/face_ml_app_ui/face_ml_user_provider.h"
#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"
#include "base/memory/raw_ref.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
// GetProfileUserNameImplementation of the ParticipantManager interface.
// Provides the FaceML App code with functions that only exist in chrome/.
//
// The ChromeFaceMLUserProvider provides information of current Chromium user
// from browser context.
class ChromeFaceMLUserProvider : public FaceMLUserProvider {
 public:
  explicit ChromeFaceMLUserProvider(content::WebUI* web_ui);

  ChromeFaceMLUserProvider(const ChromeFaceMLUserProvider&) = delete;
  ChromeFaceMLUserProvider& operator=(const ChromeFaceMLUserProvider&) = delete;

  mojom::face_ml_app::UserInformation GetCurrentUserInformation() override;

 private:
  const raw_ref<content::WebUI> web_ui_;  // Owns |this|.
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FACE_ML_CHROME_FACE_ML_USER_PROVIDER_H_
