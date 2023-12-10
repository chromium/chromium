// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/common/backend/shortcut_input_provider.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::shortcut_ui {

ShortcutsAppManager::ShortcutsAppManager(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy,
    PrefService* pref_service) {
  search_concept_registry_ =
      std::make_unique<SearchConceptRegistry>(*local_search_service_proxy);
  // Initialization of the search maps will occur on first call from
  // `OnAcceleratorsUpdated`.
  search_handler_ = std::make_unique<SearchHandler>(
      search_concept_registry_.get(), local_search_service_proxy);
  accelerator_configuration_provider_ =
      std::make_unique<AcceleratorConfigurationProvider>(pref_service);
  shortcut_input_provider_ = std::make_unique<ShortcutInputProvider>();

  accelerator_configuration_provider_->AddObserver(this);
}

ShortcutsAppManager::~ShortcutsAppManager() = default;

// This KeyedService::Shutdown method is part of a two-phase shutdown process.
// In the first phase, this Shutdown method is called, and is where we drop
// references. Once all KeyedServices are finished with the first phase, the
// services are deleted in the second phase.
void ShortcutsAppManager::Shutdown() {
  accelerator_configuration_provider_->RemoveObserver(this);
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  accelerator_configuration_provider_.reset();
  search_handler_.reset();
  search_concept_registry_.reset();
}

void ShortcutsAppManager::OnAcceleratorsUpdated(
    shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
        config) {
  SetSearchConcepts(
      std::move(config),
      accelerator_configuration_provider_->GetAcceleratorLayoutInfos());
}

void ShortcutsAppManager::SetSearchConcepts(
    shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
        config,
    std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos) {
  std::vector<SearchConcept> search_concepts;

  for (auto& layout_info : layout_infos) {
    if (const auto& config_iterator = config.find(layout_info->source);
        config_iterator != config.end()) {
      if (const auto& map_iterator =
              config_iterator->second.find(layout_info->action);
          map_iterator != config_iterator->second.end()) {
        search_concepts.emplace_back(std::move(layout_info),
                                     std::move(map_iterator->second));
      }
    }
  }

  search_concept_registry_->SetSearchConcepts(std::move(search_concepts));
}

}  // namespace ash::shortcut_ui
