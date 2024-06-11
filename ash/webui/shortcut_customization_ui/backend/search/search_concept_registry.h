// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_REGISTRY_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}  // namespace local_search_service

namespace shortcut_ui {

// This class is responsible for managing search concepts and their registration
// with the Local Search Service (LSS). This class does two main things. First,
// it processes incoming search concepts, converts them to a format readable by
// the LSS, and registers them with the LSS. Second, it maintains a map of
// search concept IDs that have been registered with the LSS and their
// associated SearchConcept so that they can be looked up via
// GetSearchConceptById.
class SearchConceptRegistry {
 public:
  // This Observer class is used to observe changes to the LSS index. When the
  // LSS index is updated, OnRegistryUpdated will be called.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnRegistryUpdated() = 0;
  };

  explicit SearchConceptRegistry(local_search_service::LocalSearchServiceProxy&
                                     local_search_service_proxy);
  SearchConceptRegistry(const SearchConceptRegistry& other) = delete;
  SearchConceptRegistry& operator=(const SearchConceptRegistry& other) = delete;
  ~SearchConceptRegistry();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Given a SearchConcept ID, return the associated SearchConcept if it has
  // been previously registered. Otherwise, return nullptr.
  const SearchConcept* GetSearchConceptById(const std::string& id) const;

  // Set SearchConcepts to be registered to the Local Search Service.
  void SetSearchConcepts(std::vector<SearchConcept> search_concepts);

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchConceptRegistryTest, AddAndRemove);
  FRIEND_TEST_ALL_PREFIXES(SearchConceptRegistryTest,
                           SearchConceptToDataStandardAccelerator);
  FRIEND_TEST_ALL_PREFIXES(SearchConceptRegistryTest,
                           SearchConceptToDataTextAccelerator);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsAppManagerTest, SetSearchConcepts);

  void NotifyRegistryUpdated();

  void SetSearchConceptsHelper(std::vector<SearchConcept> search_concepts);

  local_search_service::Data SearchConceptToData(
      const SearchConcept& search_concept);

  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  base::flat_map<std::string, SearchConcept> result_id_to_search_concept_;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<SearchConceptRegistry> weak_ptr_factory_{this};
};

}  // namespace shortcut_ui
}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_CONCEPT_REGISTRY_H_