// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace ash::shortcut_ui {

namespace {
std::string GetSearchConceptId(mojom::AcceleratorSource source,
                               uint32_t action) {
  return base::StrCat({base::NumberToString(static_cast<int>(source)), "-",
                       base::NumberToString(action)});
}
}  // namespace

SearchConcept::SearchConcept(
    ash::mojom::AcceleratorLayoutInfoPtr accelerator_layout_info,
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_infos)
    : id(GetSearchConceptId(accelerator_layout_info->source,
                            accelerator_layout_info->action)),
      accelerator_layout_info(std::move(accelerator_layout_info)),
      accelerator_infos(std::move(accelerator_infos)) {}

SearchConcept::SearchConcept(SearchConcept&& search_concept) = default;

SearchConcept& SearchConcept::operator=(SearchConcept&& search_concept) =
    default;

SearchConcept::~SearchConcept() = default;

}  // namespace ash::shortcut_ui
