// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_tag_registry.h"

#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/personalization_app/enterprise_policy_delegate.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom-shared.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::personalization_app {

namespace {

bool IsAmbientModeAllowed() {
  return ash::AmbientClient::Get() &&
         ash::AmbientClient::Get()->IsAmbientModeAllowed();
}

std::string SearchConceptToId(const SearchConcept& search_concept) {
  return base::NumberToString(base::to_underlying(search_concept.id));
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

std::vector<local_search_service::Content> SearchConceptToContentVector(
    const SearchConcept& search_concept) {
  std::vector<local_search_service::Content> content_vector;

  for (auto message_id : GetMessageIds(search_concept)) {
    content_vector.emplace_back(
        base::NumberToString(message_id),
        SearchTagRegistry::MessageIdToString(message_id));
  }

  return content_vector;
}

const SearchConcept& GetPersonalizationSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kPersonalization,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT2,
          },
      .relative_url = "",
  });
  return *search_concept;
}

const SearchConcept& GetWallpaperSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kChangeWallpaper,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER_ALT2,
          },
      .relative_url = kWallpaperSubpageRelativeUrl,
  });
  return *search_concept;
}

const SearchConcept& GetTimeOfDayWallpaperSearchConcept() {
  DCHECK(::ash::features::IsTimeOfDayWallpaperEnabled());
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kTimeOfDayWallpaper,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER_ALT3,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER_ALT4,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER_ALT5,
          },
      .relative_url =
          base::StrCat({kWallpaperSubpageRelativeUrl, "/collection?id=",
                        wallpaper_constants::kTimeOfDayWallpaperCollectionId}),
  });
  return *search_concept;
}

SearchTagRegistry::SearchConceptUpdates GetWallpaperEnterpriseUpdates(
    bool is_enterprise_managed) {
  SearchTagRegistry::SearchConceptUpdates updates{
      {&GetWallpaperSearchConcept(), !is_enterprise_managed}};
  if (::ash::features::IsTimeOfDayWallpaperEnabled()) {
    updates[&GetTimeOfDayWallpaperSearchConcept()] = !is_enterprise_managed;
  }
  return updates;
}

const SearchConcept& GetUserImageSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kChangeDeviceAccountImage,
      .message_id =
          IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT3,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT4,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT5,
          },
      .relative_url = kUserSubpageRelativeUrl,
  });
  return *search_concept;
}

SearchTagRegistry::SearchConceptUpdates GetUserImageEnterpriseUpdates(
    bool is_enterprise_managed) {
  return {{&GetUserImageSearchConcept(), !is_enterprise_managed}};
}

const SearchConcept& GetAmbientSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kAmbientMode,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE,
      .alternate_message_ids = {},
      .relative_url = kAmbientSubpageRelativeUrl,
  });
  return *search_concept;
}

const SearchConcept& GetAmbientTimeOfDaySearchConcept() {
  DCHECK(::ash::features::IsTimeOfDayScreenSaverEnabled());
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kAmbientModeTimeOfDay,
      .message_id =
          IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TIME_OF_DAY,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TIME_OF_DAY_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TIME_OF_DAY_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TIME_OF_DAY_ALT3,
          },
      .relative_url = kAmbientSubpageRelativeUrl,
  });
  return *search_concept;
}

const std::vector<SearchConcept>& GetAmbientOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
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

const SearchConcept& GetAmbientOffSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kAmbientModeTurnOn,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_ON,
      .alternate_message_ids =
          {IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TURN_ON_ALT1},
      .relative_url = kAmbientSubpageRelativeUrl,
  });
  return *search_concept;
}

SearchTagRegistry::SearchConceptUpdates GetAmbientPrefChangedUpdates(
    bool ambient_on) {
  DCHECK(IsAmbientModeAllowed());
  SearchTagRegistry::SearchConceptUpdates updates;
  for (const auto& search_concept : GetAmbientOnSearchConcepts()) {
    updates[&search_concept] = ambient_on;
  }
  updates[&GetAmbientOffSearchConcept()] = !ambient_on;
  return updates;
}

const std::vector<SearchConcept>& GetDarkModeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
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

const SearchConcept& GetDarkModeOnSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kDarkModeTurnOff,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT3,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT4,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT5,
          },
      .relative_url = "",
  });
  return *search_concept;
}

const SearchConcept& GetDarkModeOffSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
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
  });
  return *search_concept;
}

SearchTagRegistry::SearchConceptUpdates GetDarkModePrefChangedUpdates(
    bool dark_mode_on) {
  return {{&GetDarkModeOnSearchConcept(), dark_mode_on},
          {&GetDarkModeOffSearchConcept(), !dark_mode_on}};
}

const SearchConcept& GetDynamicColorSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kDynamicColor,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT3,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT4,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT5,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT6,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT7,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT8,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_DYNAMIC_COLOR_ALT9,
          },
      .relative_url = "",
  });
  return *search_concept;
}

const SearchConcept& GetKeyboardBacklightSearchConcept() {
  static const base::NoDestructor<const SearchConcept> search_concept({
      .id = mojom::SearchConceptId::kKeyboardBacklight,
      .message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT,
      .alternate_message_ids =
          {
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT1,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT2,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT3,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT4,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT5,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT6,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT7,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT8,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT9,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT10,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT11,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT12,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT13,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT14,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT15,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT16,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT17,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT18,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT19,
              IDS_PERSONALIZATION_APP_SEARCH_RESULT_KEYBOARD_BACKLIGHT_ALT20,
          },
      .relative_url = "",
  });
  return *search_concept;
}

}  // namespace

// static
std::u16string SearchTagRegistry::MessageIdToString(int message_id) {
  switch (message_id) {
    case IDS_PERSONALIZATION_APP_SEARCH_RESULT_AMBIENT_MODE_TIME_OF_DAY:
    case IDS_PERSONALIZATION_APP_SEARCH_RESULT_TIME_OF_DAY_WALLPAPER:
      return l10n_util::GetStringFUTF16(
          message_id,
          base::UTF8ToUTF16(
              AmbientBackendController::Get()->GetTimeOfDayProductName()));
    default:
      return l10n_util::GetStringUTF16(message_id);
  }
}

SearchTagRegistry::SearchTagRegistry(
    local_search_service::LocalSearchServiceProxy& local_search_service_proxy,
    PrefService* pref_service,
    std::unique_ptr<EnterprisePolicyDelegate> enterprise_policy_delegate)
    : pref_service_(pref_service),
      enterprise_policy_delegate_(std::move(enterprise_policy_delegate)) {
  DCHECK(pref_service_);
  DCHECK(enterprise_policy_delegate_);

  local_search_service_proxy.GetIndex(
      local_search_service::IndexId::kPersonalization,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());

  SearchConceptUpdates updates;
  updates[&GetPersonalizationSearchConcept()] = true;

  updates.merge(GetUserImageEnterpriseUpdates(
      enterprise_policy_delegate_->IsUserImageEnterpriseManaged()));

  updates.merge(GetWallpaperEnterpriseUpdates(
      enterprise_policy_delegate_->IsWallpaperEnterpriseManaged()));

  for (const auto& search_concept : GetDarkModeSearchConcepts()) {
    updates[&search_concept] = true;
  }
  updates.merge(GetDarkModePrefChangedUpdates(
      pref_service_->GetBoolean(ash::prefs::kDarkModeEnabled)));

  if (Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
    updates[&GetKeyboardBacklightSearchConcept()] = true;
  }

  updates[&GetDynamicColorSearchConcept()] = true;

  if (IsAmbientModeAllowed()) {
    updates[&GetAmbientSearchConcept()] = true;
    if (::ash::features::IsTimeOfDayScreenSaverEnabled()) {
      updates[&GetAmbientTimeOfDaySearchConcept()] = true;
    }
    updates.merge(GetAmbientPrefChangedUpdates(
        pref_service_->GetBoolean(::ash::ambient::prefs::kAmbientModeEnabled)));
  }

  UpdateSearchConcepts(updates);
  BindObservers();
}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::UpdateSearchConcepts(
    const SearchConceptUpdates& search_concept_updates) {
  std::vector<local_search_service::Data> data_vec;

  for (auto& [search_concept, add] : search_concept_updates) {
    std::string concept_id = SearchConceptToId(*search_concept);
    const auto it = result_id_to_search_concept_.find(concept_id);
    bool found = it != result_id_to_search_concept_.end();

    if (!found && add) {
      // Adding a search concept that was missing.
      data_vec.emplace_back(concept_id,
                            SearchConceptToContentVector(*search_concept));
      result_id_to_search_concept_[std::move(concept_id)] = search_concept;
    }

    if (found && !add) {
      // Removing a search concept that was present.
      data_vec.emplace_back(concept_id,
                            std::vector<local_search_service::Content>());
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

void SearchTagRegistry::BindObservers() {
  pref_change_registrar_.Init(pref_service_);
  // base::Unretained is safe because |this| owns |pref_change_registrar_|.
  pref_change_registrar_.Add(
      ash::prefs::kDarkModeEnabled,
      base::BindRepeating(&SearchTagRegistry::OnDarkModePrefChanged,
                          base::Unretained(this)));
  if (IsAmbientModeAllowed()) {
    // base::Unretained is safe because |this| owns |pref_change_registrar_|.
    pref_change_registrar_.Add(
        ::ash::ambient::prefs::kAmbientModeEnabled,
        base::BindRepeating(&SearchTagRegistry::OnAmbientPrefChanged,
                            base::Unretained(this)));
  }

  enterprise_policy_delegate_observation_.Observe(
      enterprise_policy_delegate_.get());
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
  UpdateSearchConcepts(GetAmbientPrefChangedUpdates(ambient_on));
}

void SearchTagRegistry::OnDarkModePrefChanged() {
  bool dark_mode_on = pref_service_->GetBoolean(::ash::prefs::kDarkModeEnabled);
  UpdateSearchConcepts(GetDarkModePrefChangedUpdates(dark_mode_on));
}

void SearchTagRegistry::OnUserImageIsEnterpriseManagedChanged(
    bool is_enterprise_managed) {
  UpdateSearchConcepts(GetUserImageEnterpriseUpdates(is_enterprise_managed));
}

void SearchTagRegistry::OnWallpaperIsEnterpriseManagedChanged(
    bool is_enterprise_managed) {
  UpdateSearchConcepts(GetWallpaperEnterpriseUpdates(is_enterprise_managed));
}

}  // namespace ash::personalization_app
