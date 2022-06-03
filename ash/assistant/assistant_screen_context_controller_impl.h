// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_screen_context_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct AssistantTree;
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class AssistantControllerImpl;

class ASH_EXPORT AssistantScreenContextControllerImpl
    : public AssistantScreenContextController,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver {
 public:
  using ScreenContextCallback =
      base::OnceCallback<void(ax::mojom::AssistantStructurePtr,
                              const std::vector<uint8_t>&)>;

  explicit AssistantScreenContextControllerImpl(
      AssistantControllerImpl* assistant_controller);

  AssistantScreenContextControllerImpl(
      const AssistantScreenContextControllerImpl&) = delete;
  AssistantScreenContextControllerImpl& operator=(
      const AssistantScreenContextControllerImpl&) = delete;

  ~AssistantScreenContextControllerImpl() override;

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(chromeos::assistant::Assistant* assistant);

  // Returns a reference to the underlying model.
  const AssistantScreenContextModel* model() const { return &model_; }

  // AssistantScreenContextController:
  void RequestScreenshot(const gfx::Rect& rect,
                         RequestScreenshotCallback callback) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      absl::optional<AssistantEntryPoint> entry_point,
      absl::optional<AssistantExitPoint> exit_point) override;

  // AssistantViewDelegateObserver:
  void OnHostViewVisibilityChanged(bool visible) override;

  void RequestScreenContext(bool include_assistant_structure,
                            const gfx::Rect& region,
                            ScreenContextCallback callback);

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  friend class AssistantScreenContextControllerTest;

  // Requests or clears the cached Assistant structure based on |visible|.
  void UpdateAssistantStructure(bool visible);

  void RequestAssistantStructure();

  // Clears the cached Assistant structure.
  void ClearAssistantStructure();

  void OnRequestAssistantStructureCompleted(
      ax::mojom::AssistantExtraPtr assistant_extra,
      std::unique_ptr<ui::AssistantTree> assistant_tree);

  void OnRequestScreenshotCompleted(bool include_assistant_structure,
                                    ScreenContextCallback callback,
                                    const std::vector<uint8_t>& screenshot);

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantService.
  chromeos::assistant::Assistant* assistant_ = nullptr;

  AssistantScreenContextModel model_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  base::WeakPtrFactory<AssistantScreenContextControllerImpl> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
