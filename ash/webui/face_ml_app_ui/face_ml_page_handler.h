// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_PAGE_HANDLER_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_PAGE_HANDLER_H_

#include "ash/webui/face_ml_app_ui/face_ml_app_ui.h"
#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class FaceMLAppUI;

// Implements the PageHandler interface.
class FaceMLPageHandler : public mojom::face_ml_app::PageHandler {
 public:
  explicit FaceMLPageHandler(FaceMLAppUI* face_ml_app_ui);
  ~FaceMLPageHandler() override;

  FaceMLPageHandler(const FaceMLPageHandler&) = delete;
  FaceMLPageHandler& operator=(const FaceMLPageHandler&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::face_ml_app::PageHandler> pending_receiver,
      mojo::PendingRemote<mojom::face_ml_app::Page> pending_page);

 private:
  void GetCurrentUserInformation(
      GetCurrentUserInformationCallback callback) override;

  mojo::Receiver<mojom::face_ml_app::PageHandler> receiver_{this};
  mojo::Remote<mojom::face_ml_app::Page> page_;
  base::raw_ref<FaceMLAppUI> face_ml_app_ui_;  // Owns |this|.
};

}  // namespace ash

#endif  // ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_PAGE_HANDLER_H_
