// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_screen_context_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class AssistantController;
class AssistantScreenContextModelObserver;

class ASH_EXPORT AssistantScreenContextController
    : public ash::mojom::AssistantScreenContextController,
      public AssistantControllerObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantScreenContextController(
      AssistantController* assistant_controller);
  ~AssistantScreenContextController() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantScreenContextController> receiver);

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns a reference to the underlying model.
  const AssistantScreenContextModel* model() const { return &model_; }

  // Adds/removes the specified screen context model |observer|.
  void AddModelObserver(AssistantScreenContextModelObserver* observer);
  void RemoveModelObserver(AssistantScreenContextModelObserver* observer);

  // ash::mojom::AssistantScreenContextController:
  void RequestScreenshot(
      const gfx::Rect& rect,
      mojom::AssistantScreenContextController::RequestScreenshotCallback
          callback) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Invoked on screen context request finished event.
  void OnScreenContextRequestFinished();

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Receiver<mojom::AssistantScreenContextController> receiver_{this};

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantScreenContextModel model_;

  // Weak pointer factory used for screen context requests.
  base::WeakPtrFactory<AssistantScreenContextController>
      screen_context_request_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
