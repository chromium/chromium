// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kShortcutNameKey[] = "shortcut_name";
const char kActiveTimeKey[] = "active_time";
const char kAuthCredentialsKey[] = "local_auth_credentials";
const char kPasswordTokenKey[] = "gaia_password_token";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kMetricsBucketIndex[] = "metrics_bucket_index";

// Local state pref to keep track of the next available profile bucket.
const char kNextMetricsBucketIndex[] = "profile.metrics.next_bucket_index";

// Returns the next available metrics bucket index and increases the index
// counter. I.e. two consecutive calls will return two consecutive numbers.
int NextAvailableMetricsBucketIndex() {
  PrefService* local_prefs = g_browser_process->local_state();
  int next_index = local_prefs->GetInteger(kNextMetricsBucketIndex);
  DCHECK_GT(next_index, 0);

  local_prefs->SetInteger(kNextMetricsBucketIndex, next_index + 1);

  return next_index;
}

}  // namespace

const base::Feature kPersistUPAInProfileInfoCache{
    "PersistUPAInProfileInfoCache", base::FEATURE_ENABLED_BY_DEFAULT};

const char ProfileAttributesEntry::kAvatarIconKey[] = "avatar_icon";
const char ProfileAttributesEntry::kBackgroundAppsKey[] = "background_apps";
const char ProfileAttributesEntry::kProfileIsEphemeral[] = "is_ephemeral";
const char ProfileAttributesEntry::kUserNameKey[] = "user_name";
const char ProfileAttributesEntry::kGAIAIdKey[] = "gaia_id";
const char ProfileAttributesEntry::kIsConsentedPrimaryAccountKey[] =
    "is_consented_primary_account";

// static
void ProfileAttributesEntry::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Bucket 0 is reserved for the guest profile, so start new bucket indices
  // at 1.
  registry->RegisterIntegerPref(kNextMetricsBucketIndex, 1);
}

ProfileAttributesEntry::ProfileAttributesEntry()
    : profile_info_cache_(nullptr),
      prefs_(nullptr),
      profile_path_(base::FilePath()) {}

// static
bool ProfileAttributesEntry::ShouldConcatenateGaiaAndProfileName() {
  return base::FeatureList::IsEnabled(features::kProfileMenuRevamp);
}

void ProfileAttributesEntry::Initialize(ProfileInfoCache* cache,
                                        const base::FilePath& path,
                                        PrefService* prefs) {
  DCHECK(!profile_info_cache_);
  DCHECK(cache);
  profile_info_cache_ = cache;

  DCHECK(profile_path_.empty());
  DCHECK(!path.empty());
  profile_path_ = path;

  DCHECK(!prefs_);
  DCHECK(prefs);
  prefs_ = prefs;

  DCHECK(profile_info_cache_->GetUserDataDir() == profile_path_.DirName());
  storage_key_ = profile_path_.BaseName().MaybeAsASCII();

  const base::Value* entry_data = GetEntryData();
  if (entry_data) {
    if (!entry_data->FindKey(kIsConsentedPrimaryAccountKey)) {
      SetBool(kIsConsentedPrimaryAccountKey,
              !GetGAIAId().empty() || !GetUserName().empty());
    }
  }

  is_force_signin_enabled_ = signin_util::IsForceSigninEnabled();
  if (is_force_signin_enabled_) {
    if (!IsAuthenticated())
      is_force_signin_profile_locked_ = true;
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)
  } else if (IsSigninRequired()) {
    // Profiles that require signin in the absence of an enterprise policy are
    // left-overs from legacy supervised users. Just unlock them, so users can
    // keep using them.
    SetLocalAuthCredentials(std::string());
    SetAuthInfo(std::string(), base::string16(), false);
    SetIsSigninRequired(false);
#endif
  }

  DCHECK(last_name_to_display_.empty());
  last_name_to_display_ = GetName();
}

base::string16 ProfileAttributesEntry::GetLocalProfileName() const {
  return GetString16(ProfileInfoCache::kNameKey);
}

base::string16 ProfileAttributesEntry::GetGAIANameToDisplay() const {
  base::string16 gaia_given_name =
      GetString16(ProfileInfoCache::kGAIAGivenNameKey);
  return gaia_given_name.empty() ? GetString16(ProfileInfoCache::kGAIANameKey)
                                 : gaia_given_name;
}

bool ProfileAttributesEntry::ShouldShowProfileLocalName(
    const base::string16& gaia_name_to_display) const {
  // Never show the profile name if it is equal to GAIA given name,
  // e.g. Matt (Matt), in that case we should only show the GAIA name.
  if (base::EqualsCaseInsensitiveASCII(gaia_name_to_display,
                                       GetLocalProfileName())) {
    return false;
  }

  // Customized profile name that is not equal to Gaia name, e.g. Matt (Work).
  if (!IsUsingDefaultName())
    return true;

  // The profile local name is a default profile name : Person n.
  std::vector<ProfileAttributesEntry*> entries =
      profile_info_cache_->GetAllProfilesAttributes();

  for (ProfileAttributesEntry* entry : entries) {
    if (entry == this)
      continue;

    base::string16 other_gaia_name_to_display = entry->GetGAIANameToDisplay();
    if (other_gaia_name_to_display.empty() ||
        other_gaia_name_to_display != gaia_name_to_display)
      continue;

    // Another profile with the same GAIA name.
    bool other_profile_name_equal_GAIA_name = base::EqualsCaseInsensitiveASCII(
        other_gaia_name_to_display, entry->GetLocalProfileName());
    // If for the other profile, the profile name is equal to GAIA name then it
    // will not be shown. For disambiguation, show for the current profile the
    // profile name even if it is Person n.
    if (other_profile_name_equal_GAIA_name)
      return true;

    bool other_is_using_default_name = entry->IsUsingDefaultName();
    // Both profiles have a default profile name,
    // e.g. Matt (Person 1), Matt (Person 2).
    if (other_is_using_default_name) {
      return true;
    }
  }
  return false;
}

base::string16 ProfileAttributesEntry::GetNameToDisplay() const {
  DCHECK(ProfileAttributesEntry::ShouldConcatenateGaiaAndProfileName);
  base::string16 name_to_display = GetGAIANameToDisplay();

  base::string16 local_profile_name = GetLocalProfileName();
  if (name_to_display.empty())
    return local_profile_name;

  if (!ShouldShowProfileLocalName(name_to_display))
    return name_to_display;

  name_to_display.append(base::UTF8ToUTF16(" ("));
  name_to_display.append(local_profile_name);
  name_to_display.append(base::UTF8ToUTF16(")"));
  return name_to_display;
}

base::string16 ProfileAttributesEntry::GetLastNameToDisplay() const {
  return last_name_to_display_;
}

bool ProfileAttributesEntry::HasProfileNameChanged() {
  base::string16 name = GetName();
  if (last_name_to_display_ == name)
    return false;

  last_name_to_display_ = name;
  return true;
}

base::string16 ProfileAttributesEntry::GetName() const {
  return ShouldConcatenateGaiaAndProfileName()
             ? GetNameToDisplay()
             : profile_info_cache_->GetNameOfProfileAtIndex(profile_index());
}

base::string16 ProfileAttributesEntry::GetShortcutName() const {
  return GetString16(kShortcutNameKey);
}

base::FilePath ProfileAttributesEntry::GetPath() const {
  return profile_path_;
}

base::Time ProfileAttributesEntry::GetActiveTime() const {
  if (IsDouble(kActiveTimeKey)) {
    return base::Time::FromDoubleT(GetDouble(kActiveTimeKey));
  } else {
    return base::Time();
  }
}

base::string16 ProfileAttributesEntry::GetUserName() const {
  return GetString16(kUserNameKey);
}

const gfx::Image& ProfileAttributesEntry::GetAvatarIcon() const {
  if (IsUsingGAIAPicture()) {
    const gfx::Image* image = GetGAIAPicture();
    if (image)
      return *image;
  }

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Use the high resolution version of the avatar if it exists. Mobile and
  // ChromeOS don't need the high resolution version so no need to fetch it.
  const gfx::Image* image = GetHighResAvatar();
  if (image)
    return *image;
#endif

  int resource_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(GetAvatarIconIndex());
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

std::string ProfileAttributesEntry::GetLocalAuthCredentials() const {
  return GetString(kAuthCredentialsKey);
}

std::string ProfileAttributesEntry::GetPasswordChangeDetectionToken() const {
  return GetString(kPasswordTokenKey);
}

bool ProfileAttributesEntry::GetBackgroundStatus() const {
  return GetBool(kBackgroundAppsKey);
}

base::string16 ProfileAttributesEntry::GetGAIAName() const {
  return profile_info_cache_->GetGAIANameOfProfileAtIndex(profile_index());
}

base::string16 ProfileAttributesEntry::GetGAIAGivenName() const {
  return profile_info_cache_->GetGAIAGivenNameOfProfileAtIndex(profile_index());
}

std::string ProfileAttributesEntry::GetGAIAId() const {
  return profile_info_cache_->GetGAIAIdOfProfileAtIndex(profile_index());
}

const gfx::Image* ProfileAttributesEntry::GetGAIAPicture() const {
  return profile_info_cache_->GetGAIAPictureOfProfileAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsUsingGAIAPicture() const {
  return profile_info_cache_->IsUsingGAIAPictureOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsGAIAPictureLoaded() const {
  return profile_info_cache_->IsGAIAPictureOfProfileAtIndexLoaded(
      profile_index());
}

bool ProfileAttributesEntry::IsSupervised() const {
  return profile_info_cache_->ProfileIsSupervisedAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsChild() const {
  return profile_info_cache_->ProfileIsChildAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsLegacySupervised() const {
  return profile_info_cache_->ProfileIsLegacySupervisedAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsOmitted() const {
  return profile_info_cache_->IsOmittedProfileAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsSigninRequired() const {
  return profile_info_cache_->ProfileIsSigninRequiredAtIndex(profile_index()) ||
         is_force_signin_profile_locked_;
}

std::string ProfileAttributesEntry::GetSupervisedUserId() const {
  return profile_info_cache_->GetSupervisedUserIdOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsEphemeral() const {
  return GetBool(kProfileIsEphemeral);
}

bool ProfileAttributesEntry::IsUsingDefaultName() const {
  return profile_info_cache_->ProfileIsUsingDefaultNameAtIndex(profile_index());
}

SigninState ProfileAttributesEntry::GetSigninState() const {
  bool is_consented_primary_account = GetBool(kIsConsentedPrimaryAccountKey);
  if (!GetGAIAId().empty() || !GetUserName().empty()) {
    return is_consented_primary_account
               ? SigninState::kSignedInWithConsentedPrimaryAccount
               : SigninState::kSignedInWithUnconsentedPrimaryAccount;
  }
  DCHECK(!is_consented_primary_account);
  return SigninState::kNotSignedIn;
}

bool ProfileAttributesEntry::IsAuthenticated() const {
  return GetBool(kIsConsentedPrimaryAccountKey);
}

bool ProfileAttributesEntry::IsUsingDefaultAvatar() const {
  return profile_info_cache_->ProfileIsUsingDefaultAvatarAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsAuthError() const {
  return GetBool(kIsAuthErrorKey);
}

size_t ProfileAttributesEntry::GetAvatarIconIndex() const {
  std::string icon_url = GetString(kAvatarIconKey);
  size_t icon_index = 0;
  if (!profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index))
    DLOG(WARNING) << "Unknown avatar icon: " << icon_url;

  return icon_index;
}

size_t ProfileAttributesEntry::GetMetricsBucketIndex() {
  int bucket_index = GetInteger(kMetricsBucketIndex);
  if (bucket_index == -1) {
    bucket_index = NextAvailableMetricsBucketIndex();
    SetInteger(kMetricsBucketIndex, bucket_index);
  }
  return bucket_index;
}

void ProfileAttributesEntry::SetLocalProfileName(const base::string16& name) {
  profile_info_cache_->SetLocalProfileNameOfProfileAtIndex(profile_index(),
                                                           name);
}

void ProfileAttributesEntry::SetShortcutName(const base::string16& name) {
  SetString16(kShortcutNameKey, name);
}

void ProfileAttributesEntry::SetActiveTimeToNow() {
  if (IsDouble(kActiveTimeKey) &&
      base::Time::Now() - GetActiveTime() < base::TimeDelta::FromHours(1)) {
    return;
  }
  SetDouble(kActiveTimeKey, base::Time::Now().ToDoubleT());
}

void ProfileAttributesEntry::SetIsOmitted(bool is_omitted) {
  profile_info_cache_->SetIsOmittedProfileAtIndex(profile_index(), is_omitted);
}

void ProfileAttributesEntry::SetSupervisedUserId(const std::string& id) {
  profile_info_cache_->SetSupervisedUserIdOfProfileAtIndex(profile_index(), id);
}

void ProfileAttributesEntry::SetLocalAuthCredentials(const std::string& auth) {
  SetString(kAuthCredentialsKey, auth);
}

void ProfileAttributesEntry::SetPasswordChangeDetectionToken(
    const std::string& token) {
  SetString(kPasswordTokenKey, token);
}

void ProfileAttributesEntry::SetBackgroundStatus(bool running_background_apps) {
  SetBool(kBackgroundAppsKey, running_background_apps);
}

void ProfileAttributesEntry::SetGAIAName(const base::string16& name) {
  profile_info_cache_->SetGAIANameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetGAIAGivenName(const base::string16& name) {
  profile_info_cache_->SetGAIAGivenNameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetGAIAPicture(gfx::Image image) {
  profile_info_cache_->SetGAIAPictureOfProfileAtIndex(profile_index(), image);
}

void ProfileAttributesEntry::SetIsUsingGAIAPicture(bool value) {
  profile_info_cache_->SetIsUsingGAIAPictureOfProfileAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsSigninRequired(bool value) {
  profile_info_cache_->SetProfileSigninRequiredAtIndex(profile_index(), value);
  if (is_force_signin_enabled_)
    LockForceSigninProfile(value);
}

void ProfileAttributesEntry::LockForceSigninProfile(bool is_lock) {
  DCHECK(is_force_signin_enabled_);
  if (is_force_signin_profile_locked_ == is_lock)
    return;
  is_force_signin_profile_locked_ = is_lock;
  profile_info_cache_->NotifyIsSigninRequiredChanged(profile_path_);
}

void ProfileAttributesEntry::SetIsEphemeral(bool value) {
  SetBool(kProfileIsEphemeral, value);
}

void ProfileAttributesEntry::SetIsUsingDefaultName(bool value) {
  profile_info_cache_->SetProfileIsUsingDefaultNameAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsUsingDefaultAvatar(bool value) {
  profile_info_cache_->SetProfileIsUsingDefaultAvatarAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsAuthError(bool value) {
  SetBool(kIsAuthErrorKey, value);
}

void ProfileAttributesEntry::SetAvatarIconIndex(size_t icon_index) {
  if (!profiles::IsDefaultAvatarIconIndex(icon_index)) {
    DLOG(WARNING) << "Unknown avatar icon index: " << icon_index;
    // switch to generic avatar
    icon_index = 0;
  }
  std::string default_avatar_icon_url =
      profiles::GetDefaultAvatarIconUrl(icon_index);
  if (default_avatar_icon_url == GetString(kAvatarIconKey)) {
    // On Windows, Taskbar and Desktop icons are refreshed every time
    // |OnProfileAvatarChanged| notification is fired.
    // As the current avatar icon is already set to |default_avatar_icon_url|,
    // it is important to avoid firing |OnProfileAvatarChanged| in this case.
    // See http://crbug.com/900374
    return;
  }

  SetString(kAvatarIconKey, default_avatar_icon_url);

  base::FilePath profile_path = GetPath();
  if (!profile_info_cache_->GetDisableAvatarDownloadForTesting()) {
    profile_info_cache_->DownloadHighResAvatarIfNeeded(icon_index,
                                                       profile_path);
  }

  profile_info_cache_->NotifyOnProfileAvatarChanged(profile_path);
}

void ProfileAttributesEntry::SetAuthInfo(const std::string& gaia_id,
                                         const base::string16& user_name,
                                         bool is_consented_primary_account) {
  // If gaia_id, username and consent state are unchanged, abort early.
  if (GetBool(kIsConsentedPrimaryAccountKey) == is_consented_primary_account &&
      gaia_id == GetGAIAId() && user_name == GetUserName()) {
    return;
  }

  const base::Value* old_data = GetEntryData();
  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetStringKey(kGAIAIdKey, gaia_id);
  new_data.SetStringKey(kUserNameKey, user_name);
  DCHECK(!is_consented_primary_account || !gaia_id.empty() ||
         !user_name.empty());
  new_data.SetBoolKey(kIsConsentedPrimaryAccountKey,
                      is_consented_primary_account);
  SetEntryData(std::move(new_data));
  profile_info_cache_->NotifyProfileAuthInfoChanged(profile_path_);
}

size_t ProfileAttributesEntry::profile_index() const {
  size_t index = profile_info_cache_->GetIndexOfProfileWithPath(profile_path_);
  DCHECK(index < profile_info_cache_->GetNumberOfProfiles());
  return index;
}

const gfx::Image* ProfileAttributesEntry::GetHighResAvatar() const {
  const size_t avatar_index = GetAvatarIconIndex();

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded.
  if (avatar_index == profiles::GetPlaceholderAvatarIndex()) {
    return &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  const std::string key =
      profiles::GetDefaultAvatarIconFileNameAtIndex(avatar_index);
  const base::FilePath image_path =
      profiles::GetPathOfHighResAvatarAtIndex(avatar_index);
  return profile_info_cache_->LoadAvatarPictureFromPath(GetPath(), key,
                                                        image_path);
}

const base::Value* ProfileAttributesEntry::GetEntryData() const {
  const base::DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  return cache->FindKeyOfType(storage_key_, base::Value::Type::DICTIONARY);
}

void ProfileAttributesEntry::SetEntryData(base::Value data) {
  DCHECK(data.is_dict());

  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetKey(storage_key_, std::move(data));
}

const base::Value* ProfileAttributesEntry::GetValue(const char* key) const {
  const base::Value* entry_data = GetEntryData();
  return entry_data ? entry_data->FindKey(key) : nullptr;
}

std::string ProfileAttributesEntry::GetString(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_string())
    return std::string();
  return value->GetString();
}

base::string16 ProfileAttributesEntry::GetString16(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_string())
    return base::string16();
  return base::UTF8ToUTF16(value->GetString());
}

double ProfileAttributesEntry::GetDouble(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_double())
    return 0.0;
  return value->GetDouble();
}

bool ProfileAttributesEntry::GetBool(const char* key) const {
  const base::Value* value = GetValue(key);
  return value && value->is_bool() && value->GetBool();
}

int ProfileAttributesEntry::GetInteger(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_int())
    return -1;
  return value->GetInt();
}

// Type checking. Only IsDouble is implemented because others do not have
// callsites.
bool ProfileAttributesEntry::IsDouble(const char* key) const {
  const base::Value* value = GetValue(key);
  return value && value->is_double();
}

// Internal setters using keys;
bool ProfileAttributesEntry::SetString(const char* key, std::string value) {
  const base::Value* old_data = GetEntryData();
  if (old_data) {
    const base::Value* old_value = old_data->FindKey(key);
    if (old_value && old_value->is_string() && old_value->GetString() == value)
      return false;
  }

  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetKey(key, base::Value(value));
  SetEntryData(std::move(new_data));
  return true;
}

bool ProfileAttributesEntry::SetString16(const char* key,
                                         base::string16 value) {
  const base::Value* old_data = GetEntryData();
  if (old_data) {
    const base::Value* old_value = old_data->FindKey(key);
    if (old_value && old_value->is_string() &&
        base::UTF8ToUTF16(old_value->GetString()) == value)
      return false;
  }

  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetKey(key, base::Value(value));
  SetEntryData(std::move(new_data));
  return true;
}

bool ProfileAttributesEntry::SetDouble(const char* key, double value) {
  const base::Value* old_data = GetEntryData();
  if (old_data) {
    const base::Value* old_value = old_data->FindKey(key);
    if (old_value && old_value->is_double() && old_value->GetDouble() == value)
      return false;
  }

  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetKey(key, base::Value(value));
  SetEntryData(std::move(new_data));
  return true;
}

bool ProfileAttributesEntry::SetBool(const char* key, bool value) {
  const base::Value* old_data = GetEntryData();
  if (old_data) {
    const base::Value* old_value = old_data->FindKey(key);
    if (old_value && old_value->is_bool() && old_value->GetBool() == value)
      return false;
  }

  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetKey(key, base::Value(value));
  SetEntryData(std::move(new_data));
  return true;
}

bool ProfileAttributesEntry::SetInteger(const char* key, int value) {
  const base::Value* old_data = GetEntryData();
  if (old_data) {
    const base::Value* old_value = old_data->FindKey(key);
    if (old_value && old_value->is_int() && old_value->GetInt() == value)
      return false;
  }

  base::Value new_data = old_data ? GetEntryData()->Clone()
                                  : base::Value(base::Value::Type::DICTIONARY);
  new_data.SetKey(key, base::Value(value));
  SetEntryData(std::move(new_data));
  return true;
}
