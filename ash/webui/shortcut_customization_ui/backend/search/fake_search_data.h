// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_FAKE_SEARCH_DATA_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_FAKE_SEARCH_DATA_H_

#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"

namespace ash::shortcut_ui::fake_search_data {

enum FakeActionIds { kAction1 = 1, kAction2 = 2, kAction3 = 3, kAction4 = 4 };

ash::mojom::AcceleratorInfoPtr CreateFakeStandardAcceleratorInfo(
    ash::mojom::AcceleratorState state);

// Default state is kEnabled.
std::vector<ash::mojom::AcceleratorInfoPtr> CreateFakeAcceleratorInfoList(
    ash::mojom::AcceleratorState state =
        ash::mojom::AcceleratorState::kEnabled);

ash::mojom::AcceleratorLayoutInfoPtr CreateFakeAcceleratorLayoutInfo(
    const std::u16string& description,
    ash::mojom::AcceleratorSource source,
    uint32_t action,
    ash::mojom::AcceleratorLayoutStyle);

std::vector<shortcut_customization::mojom::SearchResultPtr>
CreateFakeSearchResultList();

}  // namespace ash::shortcut_ui::fake_search_data

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_FAKE_SEARCH_DATA_H_