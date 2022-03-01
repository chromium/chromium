// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_screen_context_model.h"
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
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class ASH_EXPORT AssistantScreenContextControllerImpl
    : public AssistantScreenContextController {
 public:
  using ScreenContextCallback =
      base::OnceCallback<void(ax::mojom::AssistantStructurePtr,
                              const std::vector<uint8_t>&)>;

  AssistantScreenContextControllerImpl();

  AssistantScreenContextControllerImpl(
      const AssistantScreenContextControllerImpl&) = delete;
  AssistantScreenContextControllerImpl& operator=(
      const AssistantScreenContextControllerImpl&) = delete;

  ~AssistantScreenContextControllerImpl() override;

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(chromeos::assistant::Assistant* assistant);

  // AssistantScreenContextController:
  void RequestScreenshot(const gfx::Rect& rect,
                         RequestScreenshotCallback callback) override;

  void RequestScreenContext(const gfx::Rect& region,
                            ScreenContextCallback callback);

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  friend class AssistantScreenContextControllerTest;

  void OnRequestScreenshotCompleted(ScreenContextCallback callback,
                                    const std::vector<uint8_t>& screenshot);

  // Owned by AssistantService.
  chromeos::assistant::Assistant* assistant_ = nullptr;

  base::WeakPtrFactory<AssistantScreenContextControllerImpl> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_IMPL_H_
