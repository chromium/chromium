// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/knob_switch.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Conigure of grid view for `KnobSwitch` instances.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 2;
constexpr size_t kGirdViewColGroupSize = 1;

// A callback function of knob switch to show its selected state on a label.
void ShowSwitchState(views::Label* label, bool selected) {
  if (selected) {
    label->SetText(u"Switch is ON");
    return;
  }

  label->SetText(u"Switch is OFF");
}

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateKnobSwitchInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // A label used to show the selected state of a knob switch.
  auto label = std::make_unique<views::Label>();
  label->SetAccessibleName(u"switch state");
  auto knob_switch = std::make_unique<KnobSwitch>(
      base::BindRepeating(&ShowSwitchState, label.get()));
  knob_switch->SetAccessibleName(u"knob switch");

  // A disabled knob switch with selected off state.
  auto disabled_switch_off = std::make_unique<KnobSwitch>();
  disabled_switch_off->SetEnabled(false);
  disabled_switch_off->SetAccessibleName(u"disabled switch off");

  // A disabled knob switch with selected on state.
  auto disabled_switch_on = std::make_unique<KnobSwitch>();
  disabled_switch_on->SetSelected(true);
  disabled_switch_on->SetEnabled(false);
  disabled_switch_on->SetAccessibleName(u"disabled switch on");

  grid_view->AddInstance(u"", std::move(label));
  grid_view->AddInstance(u"Knob Switch", std::move(knob_switch));
  grid_view->AddInstance(u"Disabled Switch OFF",
                         std::move(disabled_switch_off));
  grid_view->AddInstance(u"Disabled Switch ON", std::move(disabled_switch_on));
  return grid_view;
}

}  // namespace ash
