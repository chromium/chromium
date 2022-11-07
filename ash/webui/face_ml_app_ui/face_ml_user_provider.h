// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_USER_PROVIDER_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_USER_PROVIDER_H_

#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"

namespace ash {
// A delegate which exposes browser functionality from //chrome to the face ml
// app ui page handler.
class FaceMLUserProvider {
 public:
  virtual ~FaceMLUserProvider() = default;

  virtual mojom::face_ml_app::UserInformation GetCurrentUserInformation() = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_USER_PROVIDER_H_
