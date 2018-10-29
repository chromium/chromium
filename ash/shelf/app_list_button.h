// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_BUTTON_H_
#define ASH_SHELF_APP_LIST_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/button/image_button.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

class AssistantOverlay;
class InkDropButtonListener;
class Shelf;
class ShelfView;

// Button used for the AppList icon on the shelf.
class ASH_EXPORT AppListButton : public views::ImageButton,
                                 public ShellObserver,
                                 public SessionObserver,
                                 public DefaultVoiceInteractionObserver {
 public:
  AppListButton(InkDropButtonListener* listener,
                ShelfView* shelf_view,
                Shelf* shelf);
  ~AppListButton() override;

  void OnAppListShown();
  void OnAppListDismissed();

  bool is_showing_app_list() const { return is_showing_app_list_; }

  // views::ImageButton:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Get the center point of the app list button circle used to draw its
  // background and ink drops.
  gfx::Point GetCenterPoint() const;

 protected:
  // views::ImageButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  void NotifyClick(const ui::Event& event) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // ShellObserver:
  void OnAppListVisibilityChanged(bool shown,
                                  aura::Window* root_window) override;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionSetupCompleted(bool completed) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  void StartVoiceInteractionAnimation();

  // Whether the voice interaction style should be used.
  bool UseVoiceInteractionStyle();

  // Initialize the voice interaction overlay.
  void InitializeVoiceInteractionOverlay();

  // True if the app list is currently showing for this display.
  // This is useful because other app_list_visible functions aren't per-display.
  bool is_showing_app_list_ = false;

  InkDropButtonListener* listener_;
  ShelfView* shelf_view_;
  Shelf* shelf_;

  // Owned by the view hierarchy. Null if the voice interaction is not enabled.
  AssistantOverlay* assistant_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> assistant_animation_delay_timer_;
  std::unique_ptr<base::OneShotTimer> assistant_animation_hide_delay_timer_;
  base::TimeTicks voice_interaction_start_timestamp_;

  mojo::Binding<mojom::VoiceInteractionObserver> voice_interaction_binding_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
