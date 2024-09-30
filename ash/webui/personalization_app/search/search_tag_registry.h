// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/personalization_app/enterprise_policy_delegate.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace personalization_app {

class SearchTagRegistry : public EnterprisePolicyDelegate::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnRegistryUpdated() = 0;
  };

  using SearchConceptUpdates = std::map<const SearchConcept*, bool>;

  static std::u16string MessageIdToString(int message_id);

  SearchTagRegistry(
      local_search_service::LocalSearchServiceProxy& local_search_service_proxy,
      PrefService* pref_service,
      std::unique_ptr<EnterprisePolicyDelegate> enterprise_policy_delegate);

  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;

  ~SearchTagRegistry() override;

  void UpdateSearchConcepts(const SearchConceptUpdates& search_concept_updates);

  const SearchConcept* GetSearchConceptById(const std::string& id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class PersonalizationAppSearchHandlerTest;

  void BindObservers();

  void OnIndexUpdateComplete(uint32_t num_deleted);

  void OnAmbientPrefChanged();
  void OnDarkModePrefChanged();

  // EnterprisePolicyDelegate::Observer:
  void OnUserImageIsEnterpriseManagedChanged(
      bool is_enterprise_managed) override;
  void OnWallpaperIsEnterpriseManagedChanged(
      bool is_enterprise_managed) override;

  base::ObserverList<Observer> observer_list_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  std::map<std::string, raw_ptr<const SearchConcept, CtnExperimental>>
      result_id_to_search_concept_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<EnterprisePolicyDelegate> enterprise_policy_delegate_;
  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<EnterprisePolicyDelegate,
                          EnterprisePolicyDelegate::Observer>
      enterprise_policy_delegate_observation_{this};
  base::WeakPtrFactory<SearchTagRegistry> weak_ptr_factory_{this};
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_TAG_REGISTRY_H_
