// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash::shortcut_ui::fake_search_data {

ash::mojom::AcceleratorInfoPtr CreateFakeStandardAcceleratorInfo(
    ash::mojom::AcceleratorState state) {
  return ash::mojom::AcceleratorInfo::New(
      /*type=*/ash::mojom::AcceleratorType::kDefault,
      /*state=*/state,
      /*locked=*/true,
      /*accelerator_locked=*/false,
      /*layout_properties=*/
      ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
          ash::mojom::StandardAcceleratorProperties::New(
              ui::Accelerator(), u"FakeKey", std::nullopt)));
}

std::vector<ash::mojom::AcceleratorInfoPtr> CreateFakeAcceleratorInfoList(
    ash::mojom::AcceleratorState state) {
  std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
  accelerator_info_list.push_back(CreateFakeStandardAcceleratorInfo(state));
  return accelerator_info_list;
}

ash::mojom::AcceleratorLayoutInfoPtr CreateFakeAcceleratorLayoutInfo(
    const std::u16string& description,
    ash::mojom::AcceleratorSource source,
    uint32_t action,
    ash::mojom::AcceleratorLayoutStyle style) {
  return ash::mojom::AcceleratorLayoutInfo::New(
      /*category=*/ash::mojom::AcceleratorCategory::kDebug,
      /*sub_category=*/ash::mojom::AcceleratorSubcategory::kGeneral,
      /*description=*/description,
      /*style=*/style,
      /*source=*/source,
      /*action=*/action);
}

std::vector<shortcut_customization::mojom::SearchResultPtr>
CreateFakeSearchResultList() {
  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results;

  search_results.push_back(shortcut_customization::mojom::SearchResult::New(
      /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"first result",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/FakeActionIds::kAction1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/CreateFakeAcceleratorInfoList(),
      /*relevance_score=*/0.5));
  search_results.push_back(shortcut_customization::mojom::SearchResult::New(
      /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"second result",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/FakeActionIds::kAction2,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/CreateFakeAcceleratorInfoList(),
      /*relevance_score=*/0.5));

  return search_results;
}

}  // namespace ash::shortcut_ui::fake_search_data