// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "ash/webui/personalization_app/search/search_concept.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/local_search_service/public/mojom/index.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/remote.h"

// TODO(https://crbug.com/1164001): move forward declaration to ash.
namespace chromeos {
namespace local_search_service {
class LocalSearchServiceProxy;
}  // namespace local_search_service
}  // namespace chromeos

class PrefService;

namespace ash {
namespace personalization_app {

class SearchTagRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnRegistryUpdated() = 0;
  };

  using SearchConceptUpdates = std::map<const SearchConcept*, bool>;

  SearchTagRegistry(::chromeos::local_search_service::LocalSearchServiceProxy&
                        local_search_service_proxy,
                    PrefService* pref_service);

  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;

  virtual ~SearchTagRegistry();

  void UpdateSearchConcepts(const SearchConceptUpdates& search_concept_updates);

  const SearchConcept* GetSearchConceptById(const std::string& id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class PersonalizationAppSearchHandlerTest;

  void OnIndexUpdateComplete(uint32_t num_deleted);

  void OnAmbientPrefChanged();
  void OnDarkModePrefChanged();

  base::ObserverList<Observer> observer_list_;
  mojo::Remote<::chromeos::local_search_service::mojom::Index> index_remote_;
  std::map<std::string, const SearchConcept*> result_id_to_search_concept_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<SearchTagRegistry> weak_ptr_factory_{this};
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_
