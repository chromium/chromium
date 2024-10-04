// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/profiles/profile_attributes_storage.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/string_compare.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/state.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_list.h"
#endif

namespace {

using ImageData = std::vector<unsigned char>;

// First eight are generic icons, which use IDS_NUMBERED_PROFILE_NAME.
const int kDefaultNames[] = {
  IDS_DEFAULT_AVATAR_NAME_8,
  IDS_DEFAULT_AVATAR_NAME_9,
  IDS_DEFAULT_AVATAR_NAME_10,
  IDS_DEFAULT_AVATAR_NAME_11,
  IDS_DEFAULT_AVATAR_NAME_12,
  IDS_DEFAULT_AVATAR_NAME_13,
  IDS_DEFAULT_AVATAR_NAME_14,
  IDS_DEFAULT_AVATAR_NAME_15,
  IDS_DEFAULT_AVATAR_NAME_16,
  IDS_DEFAULT_AVATAR_NAME_17,
  IDS_DEFAULT_AVATAR_NAME_18,
  IDS_DEFAULT_AVATAR_NAME_19,
  IDS_DEFAULT_AVATAR_NAME_20,
  IDS_DEFAULT_AVATAR_NAME_21,
  IDS_DEFAULT_AVATAR_NAME_22,
  IDS_DEFAULT_AVATAR_NAME_23,
  IDS_DEFAULT_AVATAR_NAME_24,
  IDS_DEFAULT_AVATAR_NAME_25,
  IDS_DEFAULT_AVATAR_NAME_26
};

enum class MultiProfileUserType {
  kSingleProfile,       // There is only one profile.
  kActiveMultiProfile,  // Several profiles are actively used.
  kLatentMultiProfile   // There are several profiles, but only one is actively
                        // used.
};

const char kProfileCountLastUpdatePref[] = "profile.profile_counts_reported";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
const char kLegacyProfileNameMigrated[] = "legacy.profile.name.migrated";
bool g_migration_enabled_for_testing = false;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

// Reads a PNG from disk and decodes it. If the bitmap was successfully read
// from disk then this will return the bitmap image, otherwise it will return
// an empty gfx::Image.
gfx::Image ReadBitmap(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // If the path doesn't exist, don't even try reading it.
  if (!base::PathExists(image_path))
    return gfx::Image();

  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read PNG file from disk.";
    return gfx::Image();
  }

  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(image_data)));
  if (image.IsEmpty())
    LOG(ERROR) << "Failed to decode PNG file.";

  return image;
}

// Writes |data| to disk and takes ownership of the pointer. On successful
// completion, it runs |callback|.
bool SaveBitmap(std::unique_ptr<ImageData> data,
                const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory.";
    return false;
  }

  if (!base::WriteFile(image_path, *data)) {
    LOG(ERROR) << "Failed to save image to file.";
    return false;
  }
  return true;
}

void DeleteBitmap(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DeleteFile(image_path);
}

void RunCallbackIfFileMissing(const base::FilePath& file_path,
                              base::OnceClosure callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(file_path))
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(callback));
}

// Compares two ProfileAttributesEntry using locale-sensitive comparison of
// their names. For ties, the profile path is compared next.
class ProfileAttributesSortComparator {
 public:
  ProfileAttributesSortComparator(icu::Collator* collator, bool use_local_name)
      : collator_(collator), use_local_name_(use_local_name) {}

  bool operator()(const ProfileAttributesEntry* const a,
                  const ProfileAttributesEntry* const b) const {
    UCollationResult result = base::i18n::CompareString16WithCollator(
        *collator_, GetValue(a), GetValue(b));
    if (result != UCOL_EQUAL)
      return result == UCOL_LESS;

    // If the names are the same, then compare the paths, which must be unique.
    return a->GetPath().value() < b->GetPath().value();
  }

 private:
  std::u16string GetValue(const ProfileAttributesEntry* const entry) const {
    if (use_local_name_)
      return entry->GetLocalProfileName();

    return entry->GetName();
  }

  raw_ptr<icu::Collator> collator_;
  bool use_local_name_;
};

MultiProfileUserType GetMultiProfileUserType(
    const std::vector<ProfileAttributesEntry*>& entries) {
  DCHECK_GT(entries.size(), 0u);
  if (entries.size() == 1u)
    return MultiProfileUserType::kSingleProfile;

  int active_count =
      base::ranges::count_if(entries, &ProfileMetrics::IsProfileActive);

  if (active_count <= 1)
    return MultiProfileUserType::kLatentMultiProfile;
  return MultiProfileUserType::kActiveMultiProfile;
}

profile_metrics::UnconsentedPrimaryAccountType GetUnconsentedPrimaryAccountType(
    ProfileAttributesEntry* entry) {
  if (entry->GetSigninState() == SigninState::kNotSignedIn)
    return profile_metrics::UnconsentedPrimaryAccountType::kSignedOut;
  if (entry->IsSupervised()) {
    return profile_metrics::UnconsentedPrimaryAccountType::kChild;
  }
  // TODO(crbug.com/40121889): Replace this check by
  // !entry->GetHostedDomain().has_value() in M84 (once the attributes storage
  // gets reasonably well populated).
  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          base::UTF16ToUTF8(entry->GetUserName()))) {
    return profile_metrics::UnconsentedPrimaryAccountType::kConsumer;
  }
  // TODO(crbug.com/40121889): Figure out how to distinguish EDU accounts from
  // other enterprise.
  return profile_metrics::UnconsentedPrimaryAccountType::kEnterprise;
}

void RecordProfileState(ProfileAttributesEntry* entry,
                        profile_metrics::StateSuffix suffix) {
  profile_metrics::LogProfileAccountType(
      GetUnconsentedPrimaryAccountType(entry), suffix);
  profile_metrics::LogProfileSyncEnabled(
      entry->GetSigninState() ==
          SigninState::kSignedInWithConsentedPrimaryAccount,
      suffix);
  profile_metrics::LogProfileDaysSinceLastUse(
      (base::Time::Now() - entry->GetActiveTime()).InDays(), suffix);
}

// Rotating between `from_index` to `to_index` by 1 step. Rotation is done to
// the left or the right based on the index comparison.
void Rotate(base::Value::List& list, size_t from_index, size_t to_index) {
  CHECK_LT(from_index, list.size());
  CHECK_LT(to_index, list.size());

  // Rotating left.
  if (from_index <= to_index) {
    std::rotate(list.begin() + from_index, list.begin() + from_index + 1,
                list.begin() + to_index + 1);
    return;
  }

  // Rotating right;
  // We invert the indices and work with the reverse iterator.
  size_t inv_from_index = list.size() - from_index - 1;
  size_t inv_to_index = list.size() - to_index - 1;

  std::rotate(list.rbegin() + inv_from_index,
              list.rbegin() + inv_from_index + 1,
              list.rbegin() + inv_to_index + 1);
}

}  // namespace

ProfileAttributesStorage::ProfileAttributesStorage(
    PrefService* prefs,
    const base::FilePath& user_data_dir)
    : prefs_(prefs),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      user_data_dir_(user_data_dir) {
  // Populate the attributes storage.
  ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
  base::Value::Dict& attributes = update.Get();
  for (auto kv : attributes) {
    DCHECK(kv.second.is_dict());
    base::Value::Dict& info = kv.second.GetDict();
    std::string* name = info.FindString(ProfileAttributesEntry::kNameKey);

    std::optional<bool> using_default_name =
        info.FindBool(ProfileAttributesEntry::kIsUsingDefaultNameKey);
    if (!using_default_name.has_value()) {
      // If the preference hasn't been set, and the name is default, assume
      // that the user hasn't done this on purpose.
      // |include_check_for_legacy_profile_name| is true as this is an old
      // pre-existing profile and might have a legacy default profile name.
      using_default_name = IsDefaultProfileName(
          name ? base::UTF8ToUTF16(*name) : std::u16string(),
          /*include_check_for_legacy_profile_name=*/true);
      info.Set(ProfileAttributesEntry::kIsUsingDefaultNameKey,
               using_default_name.value());
    }

    // For profiles that don't have the "using default avatar" state set yet,
    // assume it's the same as the "using default name" state.
    if (!info.FindBool(ProfileAttributesEntry::kIsUsingDefaultAvatarKey)) {
      info.Set(ProfileAttributesEntry::kIsUsingDefaultAvatarKey,
               using_default_name.value());
    }

    // `info` may become invalid after this call.
    // Profiles loaded from disk can never be omitted.
    InitEntryWithKey(kv.first, /*is_omitted=*/false);
  }

  // A profile name can depend on other profile names. Do an additional pass to
  // update last used profile names once all profiles are initialized.
  for (ProfileAttributesEntry* entry : GetAllProfilesAttributes()) {
    entry->InitializeLastNameToDisplay();
  }

  // If needed, start downloading the high-res avatars and migrate any legacy
  // profile names.
  if (!disable_avatar_download_for_testing_)
    DownloadAvatars();

#if !BUILDFLAG(IS_ANDROID)
  LoadGAIAPictureIfNeeded();
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  bool migrate_legacy_profile_names =
      (!prefs_->GetBoolean(kLegacyProfileNameMigrated) ||
       g_migration_enabled_for_testing);
  if (migrate_legacy_profile_names) {
    MigrateLegacyProfileNamesAndRecomputeIfNeeded();
    prefs_->SetBoolean(kLegacyProfileNameMigrated, true);
  }

  repeating_timer_ = std::make_unique<signin::PersistentRepeatingTimer>(
      prefs_, kProfileCountLastUpdatePref, base::Hours(24),
      base::BindRepeating(&ProfileMetrics::LogNumberOfProfiles, this));
  repeating_timer_->Start();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  EnsureProfilesOrderPrefIsInitialized();
}

ProfileAttributesStorage::~ProfileAttributesStorage() = default;

// static
void ProfileAttributesStorage::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileAttributes);
  registry->RegisterListPref(prefs::kProfilesOrder);
  registry->RegisterTimePref(kProfileCountLastUpdatePref, base::Time());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(kLegacyProfileNameMigrated, false);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
}

// static
base::flat_set<std::string> ProfileAttributesStorage::GetAllProfilesKeys(
    PrefService* local_prefs) {
  base::flat_set<std::string> profile_keys;

  const base::Value::Dict& attribute_storage =
      local_prefs->GetDict(prefs::kProfileAttributes);
  for (std::pair<const std::string&, const base::Value&> attribute_entry :
       attribute_storage) {
    profile_keys.insert(attribute_entry.first);
  }

  return profile_keys;
}

void ProfileAttributesStorage::AddProfile(ProfileAttributesInitParams params) {
  std::string key = StorageKeyFromProfilePath(params.profile_path);
  ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
  base::Value::Dict& attributes = update.Get();

  DCHECK(!params.is_consented_primary_account || !params.gaia_id.empty() ||
         !params.user_name.empty());

  base::Value::Dict info =
      base::Value::Dict()
          .Set(ProfileAttributesEntry::kNameKey, params.profile_name)
          .Set(ProfileAttributesEntry::kGAIAIdKey, params.gaia_id)
          .Set(ProfileAttributesEntry::kUserNameKey, params.user_name)
          .Set(ProfileAttributesEntry::kIsConsentedPrimaryAccountKey,
               params.is_consented_primary_account)
          .Set(ProfileAttributesEntry::kAvatarIconKey,
               profiles::GetDefaultAvatarIconUrl(params.icon_index))
          // Default value for whether background apps are running is false.
          .Set(ProfileAttributesEntry::kBackgroundAppsKey, false)
          .Set(ProfileAttributesEntry::kSupervisedUserId,
               params.supervised_user_id)
          .Set(ProfileAttributesEntry::kProfileIsEphemeral, params.is_ephemeral)
          // Either the user has provided a name manually on purpose, and in
          // this case we should not check for legacy profile names or this a
          // new profile but then it is not a legacy name, so we dont need to
          // check for legacy names.
          .Set(ProfileAttributesEntry::kIsUsingDefaultNameKey,
               IsDefaultProfileName(
                   params.profile_name,
                   /*include_check_for_legacy_profile_name*/ false))
          // Assume newly created profiles use a default avatar.
          .Set(ProfileAttributesEntry::kIsUsingDefaultAvatarKey, true)
          .Set(prefs::kSignedInWithCredentialProvider,
               params.is_signed_in_with_credential_provider);

  if (params.account_id.HasAccountIdKey()) {
    info.Set(ProfileAttributesEntry::kAccountIdKey,
             params.account_id.GetAccountIdKey());
  }

  attributes.Set(key, std::move(info));

  ScopedListPrefUpdate ordered_list_update(prefs_, prefs::kProfilesOrder);
  base::Value::List& ordered_list = ordered_list_update.Get();
  ordered_list.Append(key);

  ProfileAttributesEntry* entry = InitEntryWithKey(key, params.is_omitted);
  entry->InitializeLastNameToDisplay();

  // `OnProfileAdded()` must be the first observer method being called right
  // after a new profile is added to the storage.
  for (auto& observer : observer_list_)
    observer.OnProfileAdded(params.profile_path);

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(params.icon_index, params.profile_path);

  NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesStorage::RemoveProfileByAccountId(
    const AccountId& account_id) {
  for (ProfileAttributesEntry* entry : GetAllProfilesAttributes()) {
    bool account_id_keys_match =
        account_id.HasAccountIdKey() &&
        account_id.GetAccountIdKey() == entry->GetAccountIdKey();
    bool gaia_ids_match = !entry->GetGAIAId().empty() &&
                          account_id.GetGaiaId() == entry->GetGAIAId();
    bool user_names_match =
        !entry->GetUserName().empty() &&
        account_id.GetUserEmail() == base::UTF16ToUTF8(entry->GetUserName());
    if (account_id_keys_match || gaia_ids_match || user_names_match) {
      RemoveProfile(entry->GetPath());
      return;
    }
  }
  LOG(ERROR) << "Failed to remove profile.info_cache entry for account type "
             << static_cast<int>(account_id.GetAccountType())
             << ": matching entry not found.";
}

void ProfileAttributesStorage::RemoveProfile(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry = GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::u16string name = entry->GetName();

  for (auto& observer : observer_list_)
    observer.OnProfileWillBeRemoved(profile_path);

  ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
  base::Value::Dict& attributes = update.Get();
  std::string key = StorageKeyFromProfilePath(profile_path);
  attributes.Remove(key);
  profile_attributes_entries_.erase(profile_path.value());

  ScopedListPrefUpdate ordered_list_update(prefs_, prefs::kProfilesOrder);
  base::Value::List& ordered_list = ordered_list_update.Get();
  ordered_list.EraseValue(base::Value(key));

  // `OnProfileWasRemoved()` must be the first observer method being called
  // right after a profile was removed from the storage.
  for (auto& observer : observer_list_) {
    observer.OnProfileWasRemoved(profile_path, name);
  }

  NotifyIfProfileNamesHaveChanged();
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributes() const {
  std::vector<ProfileAttributesEntry*> ret;
  for (auto& path_and_entry : profile_attributes_entries_) {
    ProfileAttributesEntry* entry = &path_and_entry.second;
    DCHECK(entry);
    ret.push_back(entry);
  }
  return ret;
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSorted(
    bool use_local_profile_name) const {
  std::vector<ProfileAttributesEntry*> ret = GetAllProfilesAttributes();
  // Do not allocate the collator and sort if it is not necessary.
  if (ret.size() < 2)
    return ret;

  UErrorCode error_code = U_ZERO_ERROR;
  // Use the default collator. The default locale should have been properly
  // set by the time this constructor is called.
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));
  DCHECK(U_SUCCESS(error_code));

  std::sort(
      ret.begin(), ret.end(),
      ProfileAttributesSortComparator(collator.get(), use_local_profile_name));
  return ret;
}

bool ProfileAttributesStorage::IsProfilesOrderPrefValid() const {
  const base::Value::List& profile_keys_order =
      prefs_->GetList(prefs::kProfilesOrder);

  // We use this map to validate the values in the prefs.
  base::flat_map<std::string, ProfileAttributesEntry*> key_entry_map =
      GetStorageKeyEntryMap();

  // Make sure the sizes are equal to proceed.
  if (profile_keys_order.size() != key_entry_map.size()) {
    return false;
  }

  base::flat_set<ProfileAttributesEntry*> entries_set;
  for (const base::Value& keyValue : profile_keys_order) {
    const std::string& key = keyValue.GetString();

    auto key_entry_it = key_entry_map.find(key);
    // If the entry is not found, there is a mismatch.
    if (key_entry_it == key_entry_map.end()) {
      return false;
    }

    ProfileAttributesEntry* found_entry = key_entry_it->second;
    CHECK(found_entry);
    auto inserted_entry = entries_set.insert(found_entry);
    // We do not expect the same entry to be repeated or be invalid.
    // `inserted_entry.second` is false if the element already exists.
    if (!inserted_entry.second) {
      return false;
    }
  }

  return true;
}

void ProfileAttributesStorage::EnsureProfilesOrderPrefIsInitialized() {
  ScopedListPrefUpdate update(prefs_, prefs::kProfilesOrder);
  base::Value::List& profile_keys_order = update.Get();

  // If the saved order pref is not valid, we recover by reseting the whole list
  // and re-populate it with the profiles ordered by local profile name.
  if (!IsProfilesOrderPrefValid()) {
    profile_keys_order.clear();

    std::vector<ProfileAttributesEntry*> entries =
        GetAllProfilesAttributesSortedByLocalProfileName();
    for (ProfileAttributesEntry* entry : entries) {
      profile_keys_order.Append(StorageKeyFromProfilePath(entry->GetPath()));
    }
  }

  DCHECK_EQ(profile_keys_order.size(), GetNumberOfProfiles());
}

void ProfileAttributesStorage::UpdateProfilesOrderPref(size_t from_index,
                                                       size_t to_index) {
  if (from_index == to_index) {
    return;
  }

  ScopedListPrefUpdate update(prefs_, prefs::kProfilesOrder);
  base::Value::List& profile_keys_order = update.Get();

  // Apply the shift by rotating the element based on the indices.
  // Element at `from_index` will be placed at `to_index` and the rest will
  // shift left or right based on the index comparison.
  Rotate(profile_keys_order, from_index, to_index);

  base::UmaHistogramBoolean("Profile.ProfilesOrderChanged", true);
}

base::flat_map<std::string, ProfileAttributesEntry*>
ProfileAttributesStorage::GetStorageKeyEntryMap() const {
  base::flat_map<std::string, ProfileAttributesEntry*> key_entry_map;
  for (auto& path_and_entry : profile_attributes_entries_) {
    auto key = StorageKeyFromProfilePath(base::FilePath(path_and_entry.first));
    key_entry_map[key] = &path_and_entry.second;
  }
  return key_entry_map;
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedForDisplay() const {
  std::vector<ProfileAttributesEntry*> ret_ordered_entries;

  const base::Value::List& ordered_keys =
      prefs_->GetList(prefs::kProfilesOrder);
  DCHECK_EQ(ordered_keys.size(), GetNumberOfProfiles());

  base::flat_map<std::string, ProfileAttributesEntry*> key_entry_map =
      GetStorageKeyEntryMap();
  for (const base::Value& key : ordered_keys) {
    ProfileAttributesEntry* entry = key_entry_map[key.GetString()];
    DCHECK(entry);
    ret_ordered_entries.emplace_back(entry);
  }

  return ret_ordered_entries;
}

std::vector<ProfileAttributesEntry*> ProfileAttributesStorage::
    GetAllProfilesAttributesSortedByLocalProfileNameWithCheck() const {
  if (base::FeatureList::IsEnabled(kProfilesReordering)) {
    return GetAllProfilesAttributesSortedForDisplay();
  }
  return GetAllProfilesAttributesSortedByLocalProfileName();
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedByNameWithCheck()
    const {
  if (base::FeatureList::IsEnabled(kProfilesReordering)) {
    return GetAllProfilesAttributesSortedForDisplay();
  }
  return GetAllProfilesAttributesSortedByName();
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedByName() const {
  return GetAllProfilesAttributesSorted(false);
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedByLocalProfileName()
    const {
  return GetAllProfilesAttributesSorted(true);
}

ProfileAttributesEntry* ProfileAttributesStorage::GetProfileAttributesWithPath(
    const base::FilePath& path) {
  const auto entry_iter = profile_attributes_entries_.find(path.value());
  if (entry_iter == profile_attributes_entries_.end()) {
    return nullptr;
  }

  return &entry_iter->second;
}

size_t ProfileAttributesStorage::GetNumberOfProfiles() const {
  return profile_attributes_entries_.size();
}

std::u16string ProfileAttributesStorage::ChooseNameForNewProfile(
    size_t icon_index) const {
  std::u16string name;
  for (int name_index = 1;; ++name_index) {
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
    // Using native digits will break IsDefaultProfileName() below because
    // it uses sscanf.
    // TODO(jshin): fix IsDefaultProfileName to handle native digits.
    name = l10n_util::GetStringFUTF16(IDS_NEW_NUMBERED_PROFILE_NAME,
                                      base::NumberToString16(name_index));
#else
    // TODO(crbug.com/41444689): Clean up this code.
    if (icon_index < profiles::GetGenericAvatarIconCount() ||
        profiles::IsModernAvatarIconIndex(icon_index)) {
      name = l10n_util::GetStringFUTF16Int(IDS_NUMBERED_PROFILE_NAME,
                                           name_index);
    } else {
      // TODO(jshin): Check with UX if appending |name_index| to the default
      // name without a space is intended.
      name = l10n_util::GetStringUTF16(
          kDefaultNames[icon_index - profiles::GetGenericAvatarIconCount()]);
      if (name_index > 1)
        name.append(base::FormatNumber(name_index));
    }
#endif

    // Loop through previously named profiles to ensure we're not duplicating.
    std::vector<ProfileAttributesEntry*> entries =
        const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();

    if (base::ranges::none_of(entries, [name](ProfileAttributesEntry* entry) {
          return entry->GetLocalProfileName() == name ||
                 entry->GetName() == name;
        })) {
      return name;
    }
  }
}

bool ProfileAttributesStorage::IsDefaultProfileName(
    const std::u16string& name,
    bool include_check_for_legacy_profile_name) const {
  // Check whether it's one of the "Person %d" style names.
  std::u16string default_name_prefix =
      l10n_util::GetStringFUTF16(IDS_NEW_NUMBERED_PROFILE_NAME, u"");
  if (base::StartsWith(name, default_name_prefix)) {
    int generic_profile_number;  // Unused. Just a placeholder for StringToInt.
    if (base::StringToInt(name.substr(default_name_prefix.length()),
                          &generic_profile_number)) {
      return true;
    }
  }

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  if (!include_check_for_legacy_profile_name)
    return false;
#endif

  // Check if it's a "First user" old-style name.
  if (name == l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME) ||
      name == l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME))
    return true;

  // Check if it's one of the old-style profile names.
  for (int default_name : kDefaultNames) {
    if (name == l10n_util::GetStringUTF16(default_name)) {
      return true;
    }
  }
  return false;
}

size_t ProfileAttributesStorage::ChooseAvatarIconIndexForNewProfile() const {
  std::unordered_set<size_t> used_icon_indices;

  std::vector<ProfileAttributesEntry*> entries =
      const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();
  for (const ProfileAttributesEntry* entry : entries)
    used_icon_indices.insert(entry->GetAvatarIconIndex());

  return profiles::GetRandomAvatarIconIndex(used_icon_indices);
}

const gfx::Image* ProfileAttributesStorage::LoadAvatarPictureFromPath(
    const base::FilePath& profile_path,
    const std::string& key,
    const base::FilePath& image_path) const {
  // If the picture is already loaded then use it.
  if (cached_avatar_images_.count(key)) {
    if (cached_avatar_images_[key].IsEmpty())
      return nullptr;
    return &cached_avatar_images_[key];
  }

  // Don't download the image if downloading is disabled for tests.
  if (disable_avatar_download_for_testing_)
    return nullptr;

  // If the picture is already being loaded then don't try loading it again.
  if (cached_avatar_images_loading_[key])
    return nullptr;
  cached_avatar_images_loading_[key] = true;

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadBitmap, image_path),
      base::BindOnce(&ProfileAttributesStorage::OnAvatarPictureLoaded,
                     weak_ptr_factory_.GetWeakPtr(), profile_path, key));
  return nullptr;
}
bool ProfileAttributesStorage::IsGAIAPictureLoaded(
    const std::string& key) const {
  return base::Contains(cached_avatar_images_, key);
}

void ProfileAttributesStorage::SaveGAIAImageAtPath(
    const base::FilePath& profile_path,
    const std::string& key,
    gfx::Image image,
    const base::FilePath& image_path,
    const std::string& image_url_with_size) {
  cached_avatar_images_.erase(key);
  SaveAvatarImageAtPath(
      profile_path, image, key, image_path,
      base::BindOnce(&ProfileAttributesStorage::OnGAIAPictureSaved,
                     weak_ptr_factory_.GetWeakPtr(), image_url_with_size,
                     profile_path));
}

void ProfileAttributesStorage::DeleteGAIAImageAtPath(
    const base::FilePath& profile_path,
    const std::string& key,
    const base::FilePath& image_path) {
  cached_avatar_images_.erase(key);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&DeleteBitmap, image_path));
  ProfileAttributesEntry* entry = GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);
  entry->SetLastDownloadedGAIAPictureUrlWithSize(std::string());
}

void ProfileAttributesStorage::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void ProfileAttributesStorage::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

#if !BUILDFLAG(IS_ANDROID)
void ProfileAttributesStorage::RecordDeletedProfileState(
    ProfileAttributesEntry* entry) {
  DCHECK(entry);
  RecordProfileState(entry, profile_metrics::StateSuffix::kUponDeletion);
  bool is_last_profile = GetNumberOfProfiles() <= 1u;
  // If the profile has windows opened, they are still open at this moment.
  // Thus, this really means that only the profile manager is open.
  bool no_browser_windows = BrowserList::GetInstance()->empty();
  profile_metrics::LogProfileDeletionContext(is_last_profile,
                                             no_browser_windows);
}
#endif

void ProfileAttributesStorage::RecordProfilesState() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  if (entries.size() == 0)
    return;

  MultiProfileUserType type = GetMultiProfileUserType(entries);

  for (ProfileAttributesEntry* entry : entries) {
    RecordProfileState(entry, profile_metrics::StateSuffix::kAll);

    if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
      RecordProfileState(entry,
                         profile_metrics::StateSuffix::kAllManagedDevice);
    } else {
      RecordProfileState(entry,
                         profile_metrics::StateSuffix::kAllUnmanagedDevice);
    }

    switch (type) {
      case MultiProfileUserType::kSingleProfile:
        RecordProfileState(entry, profile_metrics::StateSuffix::kSingleProfile);
        break;
      case MultiProfileUserType::kActiveMultiProfile:
        RecordProfileState(entry,
                           profile_metrics::StateSuffix::kActiveMultiProfile);
        break;
      case MultiProfileUserType::kLatentMultiProfile: {
        RecordProfileState(entry,
                           profile_metrics::StateSuffix::kLatentMultiProfile);
        if (ProfileMetrics::IsProfileActive(entry)) {
          RecordProfileState(
              entry, profile_metrics::StateSuffix::kLatentMultiProfileActive);
        } else {
          RecordProfileState(
              entry, profile_metrics::StateSuffix::kLatentMultiProfileOthers);
        }
        break;
      }
    }
  }
}

void ProfileAttributesStorage::NotifyOnProfileAvatarChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileAttributesStorage::NotifyIsSigninRequiredChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileSigninRequiredChanged(profile_path);
}

void ProfileAttributesStorage::NotifyProfileAuthInfoChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileAuthInfoChanged(profile_path);
}

void ProfileAttributesStorage::NotifyIfProfileNamesHaveChanged() const {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    std::u16string old_display_name = entry->GetLastNameToDisplay();
    if (entry->HasProfileNameChanged()) {
      for (auto& observer : observer_list_)
        observer.OnProfileNameChanged(entry->GetPath(), old_display_name);
    }
  }
}

void ProfileAttributesStorage::NotifyProfileSupervisedUserIdChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileSupervisedUserIdChanged(profile_path);
}

void ProfileAttributesStorage::NotifyProfileIsOmittedChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileIsOmittedChanged(profile_path);
}

void ProfileAttributesStorage::NotifyProfileThemeColorsChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileThemeColorsChanged(profile_path);
}

void ProfileAttributesStorage::NotifyProfileHostedDomainChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileHostedDomainChanged(profile_path);
}

void ProfileAttributesStorage::NotifyOnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::NotifyProfileUserManagementAcceptanceChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileUserManagementAcceptanceChanged(profile_path);
}

void ProfileAttributesStorage::NotifyProfileManagementEnrollmentTokenChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_) {
    observer.OnProfileManagementEnrollmentTokenChanged(profile_path);
  }
}

void ProfileAttributesStorage::NotifyProfileManagementIdChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_) {
    observer.OnProfileManagementIdChanged(profile_path);
  }
}

std::string ProfileAttributesStorage::StorageKeyFromProfilePath(
    const base::FilePath& profile_path) const {
  DCHECK_EQ(user_data_dir_, profile_path.DirName());
  return profile_path.BaseName().AsUTF8Unsafe();
}

void ProfileAttributesStorage::DisableProfileMetricsForTesting() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  repeating_timer_.reset();
#endif
}

void ProfileAttributesStorage::DownloadHighResAvatarIfNeeded(
    size_t icon_index,
    const base::FilePath& profile_path) {
#if BUILDFLAG(IS_ANDROID)
  return;
#endif
  DCHECK(!disable_avatar_download_for_testing_);

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded (and it will never be
  // requested from disk by `ProfileAttributesEntry::GetHighResAvatar()`).
  if (icon_index == profiles::GetPlaceholderAvatarIndex())
    return;

  const base::FilePath& file_path =
      profiles::GetPathOfHighResAvatarAtIndex(icon_index);
  base::OnceClosure callback =
      base::BindOnce(&ProfileAttributesStorage::DownloadHighResAvatar,
                     weak_ptr_factory_.GetWeakPtr(), icon_index, profile_path);
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RunCallbackIfFileMissing, file_path,
                                std::move(callback)));
}

void ProfileAttributesStorage::DownloadHighResAvatar(
    size_t icon_index,
    const base::FilePath& profile_path) {
#if !BUILDFLAG(IS_ANDROID)
  const char* file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index);
  DCHECK(file_name);
  // If the file is already being downloaded, don't start another download.
  if (avatar_images_downloads_in_progress_.count(file_name))
    return;

  // Start the download for this file. The profile attributes storage takes
  // ownership of the avatar downloader, which will be deleted when the download
  // completes, or if that never happens, when the storage is destroyed.
  std::unique_ptr<ProfileAvatarDownloader>& current_downloader =
      avatar_images_downloads_in_progress_[file_name];
  current_downloader = std::make_unique<ProfileAvatarDownloader>(
      icon_index,
      base::BindOnce(&ProfileAttributesStorage::SaveAvatarImageAtPathNoCallback,
                     weak_ptr_factory_.GetWeakPtr(), profile_path));

  current_downloader->Start();
#endif
}

void ProfileAttributesStorage::SaveAvatarImageAtPath(
    const base::FilePath& profile_path,
    gfx::Image image,
    const std::string& key,
    const base::FilePath& image_path,
    base::OnceClosure callback) {
  cached_avatar_images_[key] = image;

  scoped_refptr<base::RefCountedMemory> png_data = image.As1xPNGBytes();
  auto data = std::make_unique<ImageData>(png_data->size());
  base::span(*data).copy_from(*png_data);

  // Remove the file from the list of downloads in progress. Note that this list
  // only contains the high resolution avatars, and not the Gaia profile images.
  auto downloader_iter = avatar_images_downloads_in_progress_.find(key);
  if (downloader_iter != avatar_images_downloads_in_progress_.end()) {
    // We mustn't delete the avatar downloader right here, since we're being
    // called by it.
    content::GetUIThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, downloader_iter->second.release());
    avatar_images_downloads_in_progress_.erase(downloader_iter);
  }

  if (data->empty()) {
    LOG(ERROR) << "Failed to PNG encode the image.";
  } else {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&SaveBitmap, std::move(data), image_path),
        base::BindOnce(&ProfileAttributesStorage::OnAvatarPictureSaved,
                       weak_ptr_factory_.GetWeakPtr(), key, profile_path,
                       std::move(callback)));
  }
}

ProfileAttributesEntry* ProfileAttributesStorage::InitEntryWithKey(
    const std::string& key,
    bool is_omitted) {
  base::FilePath path =
      user_data_dir_.Append(base::FilePath::FromUTF8Unsafe(key));

  DCHECK(!base::Contains(profile_attributes_entries_, path.value()));
  ProfileAttributesEntry* new_entry =
      &profile_attributes_entries_[path.value()];
  new_entry->Initialize(this, path, prefs_);
  new_entry->SetIsOmittedInternal(is_omitted);
  return new_entry;
}

void ProfileAttributesStorage::DownloadAvatars() {
#if !BUILDFLAG(IS_ANDROID)
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    DownloadHighResAvatarIfNeeded(entry->GetAvatarIconIndex(),
                                  entry->GetPath());
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ProfileAttributesStorage::LoadGAIAPictureIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetSigninState() == SigninState::kNotSignedIn)
      continue;

    bool is_using_GAIA_picture =
        entry->GetBool(ProfileAttributesEntry::kUseGAIAPictureKey);
    bool is_using_default_avatar = entry->IsUsingDefaultAvatar();
    // Load from disk into memory GAIA picture if it exists.
    if (is_using_GAIA_picture || is_using_default_avatar)
      entry->GetGAIAPicture();
  }
}
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
void ProfileAttributesStorage::MigrateLegacyProfileNamesAndRecomputeIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (size_t i = 0; i < entries.size(); i++) {
    std::u16string profile_name = entries[i]->GetLocalProfileName();
    if (!entries[i]->IsUsingDefaultName())
      continue;

    // Migrate any legacy profile names ("First user", "Default Profile",
    // "Saratoga", ...) to new style default names Person %n ("Person 1").
    if (!IsDefaultProfileName(
            profile_name, /*include_check_for_legacy_profile_name=*/false)) {
      entries[i]->SetLocalProfileName(
          ChooseNameForNewProfile(entries[i]->GetAvatarIconIndex()),
          /*is_default_name=*/true);
      continue;
    }

    // Current profile name is Person %n.
    // Rename duplicate default profile names, e.g.: Person 1, Person 1 to
    // Person 1, Person 2.
    for (size_t j = i + 1; j < entries.size(); j++) {
      if (profile_name == entries[j]->GetLocalProfileName()) {
        entries[j]->SetLocalProfileName(
            ChooseNameForNewProfile(entries[j]->GetAvatarIconIndex()),
            /*is_default_name=*/true);
      }
    }
  }
}

// static
void ProfileAttributesStorage::SetLegacyProfileMigrationForTesting(bool value) {
  g_migration_enabled_for_testing = value;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

void ProfileAttributesStorage::OnAvatarPictureLoaded(
    const base::FilePath& profile_path,
    const std::string& key,
    gfx::Image image) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cached_avatar_images_loading_[key] = false;
  if (cached_avatar_images_.count(key)) {
    if (!cached_avatar_images_[key].IsEmpty() || image.IsEmpty()) {
      // If GAIA picture is not empty that means that it has been set with the
      // most up-to-date value while the picture was being loaded from disk.
      // If GAIA picture is empty and the image loaded from disk is also empty
      // then there is no need to update.
      return;
    }
  }

  // Even if the image is empty (e.g. because decoding failed), place it in the
  // cache to avoid reloading it again.
  cached_avatar_images_[key] = std::move(image);

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::OnAvatarPictureSaved(
    const std::string& file_name,
    const base::FilePath& profile_path,
    base::OnceClosure callback,
    bool success) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    return;

  if (callback)
    std::move(callback).Run();

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::OnGAIAPictureSaved(
    const std::string& image_url_with_size,
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry = GetProfileAttributesWithPath(profile_path);
  // Profile could have been destroyed while saving picture to disk.
  if (entry)
    entry->SetLastDownloadedGAIAPictureUrlWithSize(image_url_with_size);
}

void ProfileAttributesStorage::SaveAvatarImageAtPathNoCallback(
    const base::FilePath& profile_path,
    gfx::Image image,
    const std::string& key,
    const base::FilePath& image_path) {
  SaveAvatarImageAtPath(profile_path, image, key, image_path,
                        base::OnceClosure());
}

void ProfileAttributesStorage::
    EnsureProfilesOrderPrefIsInitializedForTesting() {
  EnsureProfilesOrderPrefIsInitialized();
}
