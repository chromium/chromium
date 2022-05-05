// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_tag_registry.h"

#include <iterator>
#include <map>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom-shared.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace personalization_app {

namespace {

std::string SearchConceptToId(const SearchConcept& search_concept) {
  return base::NumberToString(
      static_cast<std::underlying_type_t<mojom::SearchConceptId>>(
          search_concept.id));
}

std::vector<int> GetMessageIds(const SearchConcept& search_concept) {
  std::vector<int> message_ids = {search_concept.message_id};
  for (auto alt_message_id : search_concept.alternate_message_ids) {
    if (alt_message_id == 0) {
      // |alternate_message_ids| defaults to 0 when out of message ids, stop
      // searching here.
      break;
    }
    message_ids.push_back(alt_message_id);
  }
  return message_ids;
}

std::vector<::chromeos::local_search_service::Content>
SearchConceptToContentVector(const SearchConcept& search_concept) {
  std::vector<::chromeos::local_search_service::Content> content_vector;

  for (auto message_id : GetMessageIds(search_concept)) {
    content_vector.emplace_back(base::NumberToString(message_id),
                                l10n_util::GetStringUTF16(message_id));
  }

  return content_vector;
}

const std::vector<const SearchConcept>& GetPersonalizationSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> search_concepts({
      {
          .id = mojom::SearchConceptId::kPersonalization,
          .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT2,
              },
          .relative_url = "",
      },
      {
          .id = mojom::SearchConceptId::kChangeWallpaper,
          .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER_ALT2,
              },
          .relative_url = kWallpaperSubpageRelativeUrl,
      },
      {
          .id = mojom::SearchConceptId::kChangeDeviceAccountImage,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT2,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT3,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT4,
              },
          .relative_url = kUserSubpageRelativeUrl,
      },
  });
  return *search_concepts;
}

const std::vector<const SearchConcept>& GetAmbientSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kAmbientMode,
          .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE,
          .alternate_message_ids = {},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
  });
  return *tags;
}

const std::vector<const SearchConcept>& GetAmbientOnSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kAmbientModeChooseSource,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_CHOOSE_SOURCE,
          .alternate_message_ids = {},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
      {
          .id = mojom::SearchConceptId::kAmbientModeTurnOff,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_OFF,
          .alternate_message_ids =
              {IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_OFF_ALT1},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
      {
          .id = mojom::SearchConceptId::kAmbientModeGooglePhotos,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_GOOGLE_PHOTOS_ALBUM,
          .alternate_message_ids = {},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
      {
          .id = mojom::SearchConceptId::kAmbientModeArtGallery,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_ART_GALLERY_ALBUM,
          .alternate_message_ids = {},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
  });
  return *tags;
}

const std::vector<const SearchConcept>& GetAmbientOffSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kAmbientModeTurnOn,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_ON,
          .alternate_message_ids =
              {IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_ON_ALT1},
          .relative_url = kAmbientSubpageRelativeUrl,
      },
  });
  return *tags;
}

const std::vector<const SearchConcept>& GetDarkModeSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kDarkMode,
          .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_ALT2,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_ALT3,
              },
          .relative_url = "",
      },
      {
          .id = mojom::SearchConceptId::kDarkModeSchedule,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_SCHEDULE,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_SCHEDULE_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_SCHEDULE_ALT2,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_SCHEDULE_ALT3,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_SCHEDULE_ALT4,
              },
          .relative_url = "",
      },
  });
  return *tags;
}

const std::vector<const SearchConcept>& GetDarkModeOnSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kDarkModeTurnOff,
          .message_id =
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT2,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT3,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT4,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT5,
              },
          .relative_url = "",
      },
  });
  return *tags;
}

const std::vector<const SearchConcept>& GetDarkModeOffSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>> tags({
      {
          .id = mojom::SearchConceptId::kDarkModeTurnOn,
          .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON,
          .alternate_message_ids =
              {
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT1,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT2,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT3,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT4,
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT5,
              },
          .relative_url = "",
      },
  });
  return *tags;
}

bool IsAmbientModeAllowed() {
  // TODO(b/172029925): Set up to test this code.
  return chromeos::features::IsAmbientModeEnabled() &&
         ash::AmbientClient::Get() &&
         ash::AmbientClient::Get()->IsAmbientModeAllowed();
}

}  // namespace

SearchTagRegistry::SearchTagRegistry(
    ::chromeos::local_search_service::LocalSearchServiceProxy&
        local_search_service_proxy,
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);

  local_search_service_proxy.GetIndex(
      ::chromeos::local_search_service::IndexId::kPersonalization,
      ::chromeos::local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());

  SearchConceptUpdates updates;
  for (const auto& concept : GetPersonalizationSearchConcepts()) {
    updates[&concept] = true;
  }

  if (IsAmbientModeAllowed() || ::ash::features::IsDarkLightModeEnabled()) {
    pref_change_registrar_.Init(pref_service_);
  }

  if (::ash::features::IsDarkLightModeEnabled()) {
    for (const auto& concept : GetDarkModeSearchConcepts()) {
      updates[&concept] = true;
    }
    // base::Unretained is safe because |this| owns |pref_change_registrar_|.
    pref_change_registrar_.Add(
        ash::prefs::kDarkModeEnabled,
        base::BindRepeating(&SearchTagRegistry::OnDarkModePrefChanged,
                            base::Unretained(this)));
    OnDarkModePrefChanged();
  }

  if (IsAmbientModeAllowed()) {
    for (const auto& concept : GetAmbientSearchConcepts()) {
      updates[&concept] = true;
    }
    // base::Unretained is safe because |this| owns |pref_change_registrar_|.
    pref_change_registrar_.Add(
        ::ash::ambient::prefs::kAmbientModeEnabled,
        base::BindRepeating(&SearchTagRegistry::OnAmbientPrefChanged,
                            base::Unretained(this)));
    OnAmbientPrefChanged();
  }

  UpdateSearchConcepts(updates);
}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::UpdateSearchConcepts(
    const SearchConceptUpdates& search_concept_updates) {
  std::vector<::chromeos::local_search_service::Data> data_vec;

  for (auto& [concept, add] : search_concept_updates) {
    std::string concept_id = SearchConceptToId(*concept);
    const auto it = result_id_to_search_concept_.find(concept_id);
    bool found = it != result_id_to_search_concept_.end();

    if (!found && add) {
      // Adding a search concept that was missing.
      data_vec.emplace_back(concept_id, SearchConceptToContentVector(*concept));
      result_id_to_search_concept_[std::move(concept_id)] = concept;
    }

    if (found && !add) {
      // Removing a search concept that was present.
      data_vec.emplace_back(
          concept_id, std::vector<::ash::local_search_service::Content>());
      result_id_to_search_concept_.erase(it);
    }
  }

  if (data_vec.empty()) {
    return;
  }

  index_remote_->UpdateDocuments(
      data_vec, base::BindOnce(&SearchTagRegistry::OnIndexUpdateComplete,
                               weak_ptr_factory_.GetWeakPtr()));
}

const SearchConcept* SearchTagRegistry::GetSearchConceptById(
    const std::string& id) const {
  const auto it = result_id_to_search_concept_.find(id);
  if (it == result_id_to_search_concept_.end()) {
    return nullptr;
  }
  return it->second;
}

void SearchTagRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchTagRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SearchTagRegistry::OnIndexUpdateComplete(uint32_t num_deleted) {
  DVLOG(3) << "Deleted " << num_deleted << " search concepts";
  for (auto& observer : observer_list_) {
    observer.OnRegistryUpdated();
  }
}

void SearchTagRegistry::OnAmbientPrefChanged() {
  DCHECK(IsAmbientModeAllowed());
  bool ambient_on =
      pref_service_->GetBoolean(::ash::ambient::prefs::kAmbientModeEnabled);
  SearchConceptUpdates updates;
  for (const auto& search_concept : GetAmbientOnSearchConcepts()) {
    updates[&search_concept] = ambient_on;
  }
  for (const auto& search_concept : GetAmbientOffSearchConcepts()) {
    updates[&search_concept] = !ambient_on;
  }
  UpdateSearchConcepts(updates);
}

void SearchTagRegistry::OnDarkModePrefChanged() {
  DCHECK(::ash::features::IsDarkLightModeEnabled());
  bool dark_mode_on = pref_service_->GetBoolean(::ash::prefs::kDarkModeEnabled);
  SearchConceptUpdates updates;
  for (const auto& search_concept : GetDarkModeOnSearchConcepts()) {
    updates[&search_concept] = dark_mode_on;
  }
  for (const auto& search_concept : GetDarkModeOffSearchConcepts()) {
    updates[&search_concept] = !dark_mode_on;
  }
  UpdateSearchConcepts(updates);
}

}  // namespace personalization_app
}  // namespace ash
