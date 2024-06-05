// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/metadata_table_chromeos.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

// Path to the DictionaryValue in PrefService.
constexpr char kMetadataPrefPath[] = "component_updater_metadata";

// Schema of the DictionaryValue:
// {
//   |kMetadataContentKey|:
//   {
//     {
//       |kMetadataContentItemHashedUserIdKey|: |hashed_user_id|,
//       |kMetadataContentItemComponentKey|: |component|,
//     },
//     ...
//   }
// }
//
// Key to the content (installed items) in the DictionaryValue.
constexpr char kMetadataContentKey[] = "installed_items";

// Key to the hashed user id that installs the component.
constexpr char kMetadataContentItemHashedUserIdKey[] = "hashed_user_id";
// Key to the component name.
constexpr char kMetadataContentItemComponentKey[] = "component";

// Gets current active user.
const user_manager::User* GetActiveUser() {
  DCHECK(user_manager::UserManager::Get());

  return user_manager::UserManager::Get()->GetActiveUser();
}

// Converts username to a hashed string.
//
// The result is converted to lowercase to stay compatible with
// CryptoLib::HexEncodeToBuffer().
std::string HashUsername(std::string_view username) {
  return base::ToLowerASCII(base::HexEncode(
      base::SHA1Hash(base::as_byte_span(base::ToLowerASCII(username)))));
}

const std::string& GetRequiredStringFromDict(const base::Value& dict,
                                             std::string_view key) {
  const std::string* str = dict.GetDict().FindString(key);
  DCHECK(str);
  return *str;
}

}  // namespace

MetadataTable::MetadataTable(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Load();
}

MetadataTable::~MetadataTable() = default;

// static
std::unique_ptr<component_updater::MetadataTable>
MetadataTable::CreateForTest() {
  return base::WrapUnique(new MetadataTable());
}

// static
void MetadataTable::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMetadataPrefPath);
}

bool MetadataTable::AddComponentForCurrentUser(
    const std::string& component_name) {
  const user_manager::User* active_user = GetActiveUser();
  // Return immediately if action is performed when no user is signed in.
  if (!active_user) {
    return false;
  }

  const std::string hashed_user_id =
      HashUsername(active_user->GetAccountId().GetUserEmail());
  AddItem(hashed_user_id, component_name);
  Store();
  return true;
}

bool MetadataTable::DeleteComponentForCurrentUser(
    const std::string& component_name) {
  const user_manager::User* active_user = GetActiveUser();
  // Return immediately if action is performed when no user is signed in.
  if (!active_user) {
    return false;
  }

  const std::string hashed_user_id =
      HashUsername(active_user->GetAccountId().GetUserEmail());
  if (!DeleteItem(hashed_user_id, component_name)) {
    return false;
  }
  Store();
  return true;
}

bool MetadataTable::HasComponentForAnyUser(
    const std::string& component_name) const {
  return base::ranges::any_of(
      installed_items_, [&component_name](const base::Value& item) {
        const std::string& name =
            GetRequiredStringFromDict(item, kMetadataContentItemComponentKey);
        return name == component_name;
      });
}

MetadataTable::MetadataTable() : pref_service_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MetadataTable::Load() {
  DCHECK(pref_service_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict& dict = pref_service_->GetDict(kMetadataPrefPath);
  const base::Value::List* installed_items = dict.FindList(kMetadataContentKey);
  if (installed_items) {
    installed_items_ = installed_items->Clone();
    return;
  }
  installed_items_.clear();
  Store();
}

void MetadataTable::Store() {
  DCHECK(pref_service_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedDictPrefUpdate update(pref_service_, kMetadataPrefPath);
  update->Set(kMetadataContentKey, installed_items_.Clone());
}

void MetadataTable::AddItem(const std::string& hashed_user_id,
                            const std::string& component_name) {
  if (HasComponentForUser(hashed_user_id, component_name)) {
    return;
  }

  base::Value::Dict item;
  item.Set(kMetadataContentItemHashedUserIdKey, hashed_user_id);
  item.Set(kMetadataContentItemComponentKey, component_name);
  installed_items_.Append(std::move(item));
}

bool MetadataTable::DeleteItem(const std::string& hashed_user_id,
                               const std::string& component_name) {
  size_t index = GetInstalledItemIndex(hashed_user_id, component_name);
  if (index == installed_items_.size()) {
    return false;
  }
  installed_items_.erase(installed_items_.begin() + index);
  return true;
}

bool MetadataTable::HasComponentForUser(
    const std::string& hashed_user_id,
    const std::string& component_name) const {
  return GetInstalledItemIndex(hashed_user_id, component_name) !=
         installed_items_.size();
}

size_t MetadataTable::GetInstalledItemIndex(
    const std::string& hashed_user_id,
    const std::string& component_name) const {
  for (size_t i = 0; i < installed_items_.size(); ++i) {
    const auto& dict = installed_items_[i];
    const std::string& user_id =
        GetRequiredStringFromDict(dict, kMetadataContentItemHashedUserIdKey);
    if (user_id != hashed_user_id) {
      continue;
    }
    const std::string& name =
        GetRequiredStringFromDict(dict, kMetadataContentItemComponentKey);
    if (name != component_name) {
      continue;
    }
    return i;
  }
  return installed_items_.size();
}

}  // namespace component_updater
