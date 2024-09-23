// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_FACTORIES_H_
#define ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_FACTORIES_H_

#include <memory>

namespace ash {

class SystemUIComponentsGridView;

// The factories of `SystemUIComponentsGridView` for different system UI
// components in ash/style/.
std::unique_ptr<SystemUIComponentsGridView> CreateIconButtonInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreatePillButtonInstancesGirdView();
std::unique_ptr<SystemUIComponentsGridView> CreateCheckboxInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView>
CreateCheckboxGroupInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView>
CreateRadioButtonInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView>
CreateRadioButtonGroupInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreateSwitchInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreateTabSliderInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView>
CreateSystemTextfieldInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreatePaginationInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreateTypographyInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreateComboboxInstancesGridView();
std::unique_ptr<SystemUIComponentsGridView> CreateCutoutsGridView();

}  // namespace ash

#endif  // ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_FACTORIES_H_
