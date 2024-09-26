// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_
#define ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_

#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::graduation {

// WebUI handler for the Graduation App.
class GraduationUiHandler : public graduation_ui::mojom::GraduationUiHandler {
 public:
  explicit GraduationUiHandler(
      mojo::PendingReceiver<graduation_ui::mojom::GraduationUiHandler>
          receiver);

  GraduationUiHandler(const GraduationUiHandler&) = delete;
  GraduationUiHandler& operator=(const GraduationUiHandler&) = delete;
  ~GraduationUiHandler() override;

  // graduation_ui::mojom::GraduationUiHandler:
  void GetProfileInfo(GetProfileInfoCallback callback) override;

 private:
  friend class GraduationUiHandlerTest;

  mojo::Receiver<graduation_ui::mojom::GraduationUiHandler> receiver_;
};

}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_
