// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_

#include "ash/ash_export.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/system/palette/common_palette_tool.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace ash {

// This will be used for the UMA stats to note deprecation toast events
// for Assistant stylus features.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Also remember to update the
// DeprecateStylusFeaturesToastEvent enum listing in
// tools/metrics/histograms/enums.xml.
enum DeprecateStylusFeaturesToastEvent {
  // Features not deprecated, toast not shown.
  kNotDeprecatedToastNotShown = 0,
  // Features deprecated, toast shown (first time).
  kDeprecatedToastShown = 1,
  // Features deprecated, toast not shown (already shown).
  kDeprecatedToastNotShown = 2,

  kMaxValue = kDeprecatedToastNotShown
};

// A palette tool that lets the user select a screen region to be passed
// to the Assistant framework.
//
// Unlike other palette tools, it can be activated not only through the stylus
// menu, but also by the stylus button click.
class ASH_EXPORT MetalayerMode : public CommonPaletteTool,
                                 public ui::EventHandler,
                                 public AssistantStateObserver,
                                 public HighlighterController::Observer {
 public:
  explicit MetalayerMode(Delegate* delegate);

  MetalayerMode(const MetalayerMode&) = delete;
  MetalayerMode& operator=(const MetalayerMode&) = delete;

  ~MetalayerMode() override;

 private:
  // Whether the metalayer feature is enabled by the user. This is different
  // from |enabled| which means that the palette tool is currently selected by
  // the user.
  bool feature_enabled() const {
    return assistant_enabled_ && assistant_context_enabled_ &&
           assistant_allowed_state_ ==
               assistant::AssistantAllowedState::ALLOWED;
  }

  // Whether the tool is in "loading" state.
  bool loading() const {
    return feature_enabled() &&
           assistant_status_ == assistant::AssistantStatus::NOT_READY;
  }

  // Whether the tool can be selected from the menu (only true when enabled
  // by the user and fully loaded).
  bool selectable() const {
    return feature_enabled() &&
           assistant_status_ != assistant::AssistantStatus::NOT_READY;
  }

  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(assistant::AssistantStatus status) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      assistant::AssistantAllowedState state) override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;

  // Update the state of the tool based on the current availability of the tool.
  void UpdateState();

  // Update the palette menu item based on the current availability of the tool.
  void UpdateView();

  // Called when the metalayer session is complete.
  void OnMetalayerSessionComplete();

  assistant::AssistantStatus assistant_status_ =
      assistant::AssistantStatus::NOT_READY;

  bool assistant_enabled_ = false;

  bool assistant_context_enabled_ = false;

  assistant::AssistantAllowedState assistant_allowed_state_ =
      assistant::AssistantAllowedState::ALLOWED;

  base::TimeTicks previous_stroke_end_;

  // True when the mode is activated via the stylus barrel button.
  bool activated_via_button_ = false;

  base::WeakPtrFactory<MetalayerMode> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
