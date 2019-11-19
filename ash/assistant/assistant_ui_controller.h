// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_screen_context_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget_observer.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class Assistant;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AssistantContainerView;
class AssistantController;

class ASH_EXPORT AssistantUiController
    : public views::WidgetObserver,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantScreenContextModelObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver,
      public CaptionBarDelegate,
      public HighlighterController::Observer,
      public KeyboardControllerObserver,
      public display::DisplayObserver,
      public ui::EventObserver {
 public:
  explicit AssistantUiController(AssistantController* assistant_controller);
  ~AssistantUiController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns the underlying model.
  const AssistantUiModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantUiModelObserver* observer);
  void RemoveModelObserver(AssistantUiModelObserver* observer);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnMicStateChanged(MicState mic_state) override;

  // AssistantScreenContextModelObserver:
  void OnScreenContextRequestStateChanged(
      ScreenContextRequestState request_state) override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(AssistantButtonId id) override;

  // AssistantViewDelegateObserver:
  void OnDialogPlateButtonPressed(AssistantButtonId id) override;
  void OnMiniViewPressed() override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;
  void OnOpeningUrl(const GURL& url,
                    bool in_background,
                    bool from_server) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  void ShowUi(AssistantEntryPoint entry_point);
  void HideUi(AssistantExitPoint exit_point);
  void CloseUi(AssistantExitPoint exit_point);
  void ToggleUi(base::Optional<AssistantEntryPoint> entry_point,
                base::Optional<AssistantExitPoint> exit_point);

  AssistantContainerView* GetViewForTest();

 private:
  // Updates UI mode to |ui_mode| if specified. Otherwise UI mode is updated on
  // the basis of interaction/widget visibility state. If |due_to_interaction|
  // is true, the UI mode changed because of an Assistant interaction.
  void UpdateUiMode(base::Optional<AssistantUiMode> ui_mode = base::nullopt,
                    bool due_to_interaction = false);

  // Calculate and update the usable work area.
  void UpdateUsableWorkArea(aura::Window* root_window);

  // Constructs/resets |container_view_|.
  void CreateContainerView();
  void ResetContainerView();

  // Adds/removes observers used for calculating usable work area as needed.
  void UpdateUsableWorkAreaObservers();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantUiModel model_;

  // Owned by view hierarchy.
  AssistantContainerView* container_view_ = nullptr;

  std::unique_ptr<views::EventMonitor> event_monitor_;

  gfx::Rect keyboard_workspace_occluded_bounds_;

  // When hidden, Assistant automatically closes itself to finish the previous
  // session. We delay this behavior to allow the user an opportunity to resume.
  base::OneShotTimer auto_close_timer_;

  // Whether the UI controller is observing changes to the usable work area.
  bool is_observing_usable_work_area_ = false;

  base::WeakPtrFactory<AssistantUiController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantUiController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
