// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
#include "base/callback_forward.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class ASH_EXPORT AssistantScreenContextControllerImpl
    : public AssistantScreenContextController {
 public:
  AssistantScreenContextControllerImpl();

  AssistantScreenContextControllerImpl(
      const AssistantScreenContextControllerImpl&) = delete;
  AssistantScreenContextControllerImpl& operator=(
      const AssistantScreenContextControllerImpl&) = delete;

  ~AssistantScreenContextControllerImpl() override;

  // AssistantScreenContextController:
  void RequestScreenshot(const gfx::Rect& rect,
                         RequestScreenshotCallback callback) override;

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  friend class AssistantScreenContextControllerTest;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
