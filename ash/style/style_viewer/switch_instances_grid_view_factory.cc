// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "ash/style/switch.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Conigure of grid view for `KnobSwitch` instances.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 2;
constexpr size_t kGirdViewColGroupSize = 1;

// A callback function of knob switch to show its selected state on a label.
void ShowSwitchState(views::Label* label, Switch* switch_view) {
  if (switch_view->GetIsOn()) {
    label->SetText(u"Switch is ON");
    return;
  }

  label->SetText(u"Switch is OFF");
}

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateSwitchInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // A label used to show the selected state of a knob switch.
  auto label = std::make_unique<views::Label>(u"Switch is OFF");
  label->GetViewAccessibility().SetName(u"switch state");
  auto switch_view = std::make_unique<Switch>();
  switch_view->SetCallback(
      base::BindRepeating(&ShowSwitchState, label.get(), switch_view.get()));
  switch_view->GetViewAccessibility().SetName(u"switch");

  // A disabled knob switch with selected off state.
  auto disabled_switch_off = std::make_unique<Switch>();
  disabled_switch_off->SetEnabled(false);
  disabled_switch_off->GetViewAccessibility().SetName(u"disabled switch off");

  // A disabled knob switch with selected on state.
  auto disabled_switch_on = std::make_unique<Switch>();
  disabled_switch_on->SetIsOn(true);
  disabled_switch_on->SetEnabled(false);
  disabled_switch_on->GetViewAccessibility().SetName(u"disabled switch on");

  grid_view->AddInstance(u"", std::move(label));
  grid_view->AddInstance(u"Switch", std::move(switch_view));
  grid_view->AddInstance(u"Disabled Switch OFF",
                         std::move(disabled_switch_off));
  grid_view->AddInstance(u"Disabled Switch ON", std::move(disabled_switch_on));
  return grid_view;
}

}  // namespace ash
