// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_H_

#include <string>
#include <vector>

#include "ash/public/mojom/accelerator_info.mojom.h"

namespace ash::shortcut_ui {

// Represents a potential search result. SearchConcepts are 1:1 with a specific
// shortcut and all of its accelerators. SearchConcepts are processed by the
// SearchConceptRegistry and then registered with the Local Search Service index
// so that they can be searched for.
struct SearchConcept {
  SearchConcept(ash::mojom::AcceleratorLayoutInfoPtr accelerator_layout_info,
                std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_infos);
  SearchConcept(SearchConcept&&);
  SearchConcept& operator=(SearchConcept&& search_concept);
  ~SearchConcept();

  // The unique ID for a SearchConcept, created on construction.
  // It is formed as a concatenation of the AcceleratorLayoutInfo source +
  // action id.
  std::string id;

  // The shortcut's AcceleratorLayoutInfo contains the description, which
  // is used to populate the search index.
  //
  // The entire layout_info is included so that the SearchHandler can
  // construct a SearchResult from a SearchConcept without performing
  // additional lookups.
  mojom::AcceleratorLayoutInfoPtr accelerator_layout_info;

  // A list of all accelerator_infos for this shortcut. The shortcut keys, or
  // the full text in the case of a text accelerator, are used to populate the
  // search index.
  std::vector<mojom::AcceleratorInfoPtr> accelerator_infos;
};

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_H_