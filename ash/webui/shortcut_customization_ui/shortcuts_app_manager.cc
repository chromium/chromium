// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
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
  search_handler_ = std::make_unique<SearchHandler>(
      search_concept_registry_.get(), local_search_service_proxy);
  accelerator_configuration_provider_ =
      std::make_unique<AcceleratorConfigurationProvider>(pref_service);

  accelerator_configuration_provider_->AddObserver(this);

  // This sets the initial search concepts after the
  // AcceleratorConfigurationProvider has finished construction. Future updates
  // to the search registry are handled by the OnAcceleratorsUpdated observer.
  SetSearchConcepts(
      accelerator_configuration_provider_->GetAcceleratorConfig(),
      accelerator_configuration_provider_->GetAcceleratorLayoutInfos());
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
        // Filter accelerators that state is 'kDisabledByUser' from
        // map_iterator->second
        auto& accelerators = map_iterator->second;
        accelerators.erase(
            std::remove_if(accelerators.begin(), accelerators.end(),
                           [](const auto& accel_ptr) {
                             return accel_ptr->state ==
                                    mojom::AcceleratorState::kDisabledByUser;
                           }),
            accelerators.end());
        if (!accelerators.empty()) {
          search_concepts.emplace_back(std::move(layout_info),
                                       std::move(accelerators));
        }
      }
    }
  }

  search_concept_registry_->SetSearchConcepts(std::move(search_concepts));
}

}  // namespace ash::shortcut_ui
