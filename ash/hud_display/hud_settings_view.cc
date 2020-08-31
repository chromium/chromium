// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_settings_view.h"

#include "ash/hud_display/hud_properties.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/env.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {

class HUDCheckboxHandler {
 public:
  HUDCheckboxHandler(
      views::Checkbox* checkbox,
      base::RepeatingCallback<void(views::Checkbox*)> update_state,
      base::RepeatingCallback<void(views::Checkbox*)> handle_click)
      : checkbox_(checkbox),
        update_state_(update_state),
        handle_click_(handle_click) {}

  HUDCheckboxHandler(const HUDCheckboxHandler&) = delete;
  HUDCheckboxHandler& operator=(const HUDCheckboxHandler&) = delete;

  void UpdateState() const { update_state_.Run(checkbox_); }
  void HandleClick() const { handle_click_.Run(checkbox_); }

  const views::Checkbox* checkbox() const { return checkbox_; }

 private:
  views::Checkbox* const checkbox_;  // not owned.
  base::RepeatingCallback<void(views::Checkbox*)> update_state_;
  base::RepeatingCallback<void(views::Checkbox*)> handle_click_;
};

namespace {

base::RepeatingCallback<void(views::Checkbox*)> GetUpdateStateCallback(
    const bool viz::DebugRendererSettings::*field) {
  return base::BindRepeating(
      [](const bool viz::DebugRendererSettings::*field,
         views::Checkbox* checkbox) {
        checkbox->SetChecked(aura::Env::GetInstance()
                                 ->context_factory()
                                 ->GetHostFrameSinkManager()
                                 ->debug_renderer_settings().*
                             field);
      },
      field);
}

base::RepeatingCallback<void(views::Checkbox*)> GetHandleClickCallback(
    bool viz::DebugRendererSettings::*field) {
  return base::BindRepeating(
      [](bool viz::DebugRendererSettings::*field, views::Checkbox* checkbox) {
        viz::HostFrameSinkManager* manager = aura::Env::GetInstance()
                                                 ->context_factory()
                                                 ->GetHostFrameSinkManager();
        viz::DebugRendererSettings debug_settings =
            manager->debug_renderer_settings();
        debug_settings.*field = checkbox->GetChecked();
        manager->UpdateDebugRendererSettings(debug_settings);
      },
      field);
}

}  // anonymous namespace

BEGIN_METADATA(HUDSettingsView, View)
END_METADATA

HUDSettingsView::HUDSettingsView() {
  SetVisible(false);

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetBorder(views::CreateSolidBorder(1, kHUDDefaultColor));

  auto add_checkbox = [](HUDSettingsView* self,
                         const base::string16& text) -> views::Checkbox* {
    views::Checkbox* checkbox =
        self->AddChildView(std::make_unique<views::Checkbox>(text, self));
    checkbox->SetEnabledTextColors(kHUDDefaultColor);
    checkbox->SetProperty(kHUDClickHandler, HTCLIENT);
    return checkbox;
  };

  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, base::ASCIIToUTF16("Tint composited content")),
      GetUpdateStateCallback(
          &viz::DebugRendererSettings::tint_composited_content),
      GetHandleClickCallback(
          &viz::DebugRendererSettings::tint_composited_content)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, base::ASCIIToUTF16("Show overdraw feedback")),
      GetUpdateStateCallback(
          &viz::DebugRendererSettings::show_overdraw_feedback),
      GetHandleClickCallback(
          &viz::DebugRendererSettings::show_overdraw_feedback)));
  checkbox_handlers_.push_back(std::make_unique<HUDCheckboxHandler>(
      add_checkbox(this, base::ASCIIToUTF16("Show aggregated damage")),
      GetUpdateStateCallback(
          &viz::DebugRendererSettings::show_aggregated_damage),
      GetHandleClickCallback(
          &viz::DebugRendererSettings::show_aggregated_damage)));
}

HUDSettingsView::~HUDSettingsView() = default;

void HUDSettingsView::ButtonPressed(views::Button* sender,
                                    const ui::Event& /*event*/) {
  for (const auto& handler : checkbox_handlers_) {
    if (sender != handler->checkbox())
      continue;

    handler->HandleClick();
    break;
  }
}

void HUDSettingsView::ToggleVisibility() {
  const bool is_shown = !GetVisible();
  if (is_shown) {
    for (const auto& handler : checkbox_handlers_) {
      handler->UpdateState();
    }
  }
  SetVisible(is_shown);
}

}  // namespace hud_display

}  // namespace ash
