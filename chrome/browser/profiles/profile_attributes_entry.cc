// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_attributes_entry.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/state.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_properties.h"  // nogncheck crbug.com/1125897
#endif

namespace {

const char kGAIAGivenNameKey[] = "gaia_given_name";
const char kGAIANameKey[] = "gaia_name";
const char kShortcutNameKey[] = "shortcut_name";
const char kActiveTimeKey[] = "active_time";
const char kMetricsBucketIndex[] = "metrics_bucket_index";
const char kForceSigninProfileLockedKey[] = "force_signin_profile_locked";
const char kHostedDomain[] = "hosted_domain";
const char kOIDCIdentityNameKey[] = "oidc_identity_name";
const char kProfileManagementEnrollmentToken[] =
    "profile_management_enrollment_token";
const char kDasherlessManagement[] = "dasherless_management";
const char kProfileManagementOidcAuthToken[] =
    "profile_management_oidc_auth_token";
const char kProfileManagementOidcIdToken[] = "profile_management_oidc_id_token";
const char kProfileManagementId[] = "profile_management_id";
const char kProfileManagementOidcState[] = "profile_management_oidc_state";
const char kUserAcceptedAccountManagement[] =
    "user_accepted_account_management";
const char kIsUsingNewPlaceholderAvatarIcon[] =
    "is_using_new_placeholder_avatar_icon";

// All accounts info. This is a dictionary containing sub-dictionaries of
// account information, keyed by the gaia ID. The sub-dictionaries are empty for
// now but can be populated in the future. Example for two accounts:
//
// "all_accounts": {
//   "gaia_id1": {},
//   "gaia_id2": {}
// }
const char kAllAccountsKey[] = "all_accounts";

// Avatar info.
const char kLastDownloadedGAIAPictureUrlWithSizeKey[] =
    "last_downloaded_gaia_picture_url_with_size";
const char kGAIAPictureFileNameKey[] = "gaia_picture_file_name";

// Profile colors info.
const char kProfileHighlightColorKey[] = "profile_highlight_color";
const char kDefaultAvatarFillColorKey[] = "default_avatar_fill_color";
const char kDefaultAvatarStrokeColorKey[] = "default_avatar_stroke_color";
const char kProfileColorSeedKey[] = "profile_color_seed";

// Low-entropy accounts info, for metrics only.
const char kFirstAccountNameHash[] = "first_account_name_hash";
const char kHasMultipleAccountNames[] = "has_multiple_account_names";

// Local state pref to keep track of the next available profile bucket.
const char kNextMetricsBucketIndex[] = "profile.metrics.next_bucket_index";

// Deprecated 3/2023.
const char kAccountCategories[] = "account_categories";

constexpr int kIntegerNotSet = -1;

// Number of distinct low-entropy hash values. Changing this value invalidates
// existing persisted hashes.
constexpr int kNumberOfLowEntropyHashValues = 1024;

// Returns the next available metrics bucket index and increases the index
// counter. I.e. two consecutive calls will return two consecutive numbers.
int NextAvailableMetricsBucketIndex() {
  PrefService* local_prefs = g_browser_process->local_state();
  int next_index = local_prefs->GetInteger(kNextMetricsBucketIndex);
  DCHECK_GT(next_index, 0);

  local_prefs->SetInteger(kNextMetricsBucketIndex, next_index + 1);

  return next_index;
}

int GetLowEntropyHashValue(const std::string& value) {
  return base::PersistentHash(value) % kNumberOfLowEntropyHashValues;
}

}  // namespace

using profiles::PlaceholderAvatarIconParams;

const char ProfileAttributesEntry::kSupervisedUserId[] = "managed_user_id";
const char ProfileAttributesEntry::kAvatarIconKey[] = "avatar_icon";
const char ProfileAttributesEntry::kBackgroundAppsKey[] = "background_apps";
const char ProfileAttributesEntry::kProfileIsEphemeral[] = "is_ephemeral";
const char ProfileAttributesEntry::kUserNameKey[] = "user_name";
const char ProfileAttributesEntry::kGAIAIdKey[] = "gaia_id";
const char ProfileAttributesEntry::kIsConsentedPrimaryAccountKey[] =
    "is_consented_primary_account";
const char ProfileAttributesEntry::kNameKey[] = "name";
const char ProfileAttributesEntry::kIsUsingDefaultNameKey[] =
    "is_using_default_name";
const char ProfileAttributesEntry::kIsUsingDefaultAvatarKey[] =
    "is_using_default_avatar";
const char ProfileAttributesEntry::kUseGAIAPictureKey[] = "use_gaia_picture";
const char ProfileAttributesEntry::kAccountIdKey[] = "account_id_key";

// static
void ProfileAttributesEntry::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Bucket 0 is reserved for the guest profile, so start new bucket indices
  // at 1.
  registry->RegisterIntegerPref(kNextMetricsBucketIndex, 1);
}

ProfileAttributesEntry::ProfileAttributesEntry() = default;

ProfileManagementOidcTokens::ProfileManagementOidcTokens() {}

ProfileManagementOidcTokens::ProfileManagementOidcTokens(
    const std::string& auth_token,
    const std::string& id_token,
    const std::u16string& identity_name)
    : auth_token(auth_token),
      id_token(id_token),
      identity_name(identity_name) {}

ProfileManagementOidcTokens::ProfileManagementOidcTokens(
    const std::string& auth_token,
    const std::string& id_token,
    const std::string& state)
    : auth_token(auth_token), id_token(id_token), state(state) {}

ProfileManagementOidcTokens::ProfileManagementOidcTokens(
    ProfileManagementOidcTokens&& other) = default;
ProfileManagementOidcTokens& ProfileManagementOidcTokens::operator=(
    ProfileManagementOidcTokens&& other) = default;

ProfileManagementOidcTokens::ProfileManagementOidcTokens(
    const ProfileManagementOidcTokens& other) = default;
ProfileManagementOidcTokens& ProfileManagementOidcTokens::operator=(
    const ProfileManagementOidcTokens& other) = default;

ProfileManagementOidcTokens::~ProfileManagementOidcTokens() = default;

void ProfileAttributesEntry::Initialize(ProfileAttributesStorage* storage,
                                        const base::FilePath& path,
                                        PrefService* prefs) {
  DCHECK(!profile_attributes_storage_);
  DCHECK(storage);
  profile_attributes_storage_ = storage;

  DCHECK(profile_path_.empty());
  DCHECK(!path.empty());
  profile_path_ = path;

  DCHECK(!prefs_);
  DCHECK(prefs);
  prefs_ = prefs;

  storage_key_ =
      profile_attributes_storage_->StorageKeyFromProfilePath(profile_path_);

  MigrateObsoleteProfileAttributes();

  const base::Value::Dict* entry_data = GetEntryData();
  if (entry_data) {
    if (!entry_data->contains(kIsConsentedPrimaryAccountKey)) {
      SetBool(kIsConsentedPrimaryAccountKey,
              !GetGAIAId().empty() || !GetUserName().empty());
    }
  }

  if (signin_util::IsForceSigninEnabled()) {
    if (!CanBeManaged())
      SetBool(kForceSigninProfileLockedKey, true);
  } else {
    // Reset the locked state to avoid a profile being locked after the force
    // signin policy has been disabled.
    SetBool(kForceSigninProfileLockedKey, false);
  }
}

void ProfileAttributesEntry::InitializeLastNameToDisplay() {
  DCHECK(last_name_to_display_.empty());
  last_name_to_display_ = GetName();
}

std::u16string ProfileAttributesEntry::GetLocalProfileName() const {
  return GetString16(kNameKey);
}

std::u16string ProfileAttributesEntry::GetGAIANameToDisplay() const {
  std::u16string gaia_given_name = GetGAIAGivenName();
  return gaia_given_name.empty() ? GetGAIAName() : gaia_given_name;
}

bool ProfileAttributesEntry::ShouldShowProfileLocalName(
    const std::u16string& gaia_name_to_display) const {
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
      profile_attributes_storage_->GetAllProfilesAttributes();

  for (ProfileAttributesEntry* entry : entries) {
    if (entry == this)
      continue;

    std::u16string other_gaia_name_to_display = entry->GetGAIANameToDisplay();
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

bool ProfileAttributesEntry::ShouldUpdateGAIAPicture(
    const std::string& image_url_with_size,
    bool image_is_empty) const {
  std::string old_file_name = GetString(kGAIAPictureFileNameKey);
  if (old_file_name.empty() && image_is_empty) {
    // On Windows, Taskbar and Desktop icons are refreshed every time
    // |OnProfileAvatarChanged| notification is fired.
    // Updating from an empty image to a null image is a no-op and it is
    // important to avoid firing |OnProfileAvatarChanged| in this case.
    // See http://crbug.com/900374
    DCHECK(!IsGAIAPictureLoaded());
    return false;
  }

  std::string current_gaia_image_url =
      GetLastDownloadedGAIAPictureUrlWithSize();
  if (old_file_name.empty() || image_is_empty ||
      current_gaia_image_url != image_url_with_size) {
    return true;
  }
  const gfx::Image* gaia_picture = GetGAIAPicture();
  if (gaia_picture && !gaia_picture->IsEmpty()) {
    return false;
  }

  // We either did not load the GAIA image or we failed to. In that case, only
  // update if the GAIA picture is used as the profile avatar.
  return IsUsingDefaultAvatar() || IsUsingGAIAPicture();
}

std::u16string ProfileAttributesEntry::GetLastNameToDisplay() const {
  return last_name_to_display_;
}

bool ProfileAttributesEntry::HasProfileNameChanged() {
  std::u16string name = GetName();
  if (last_name_to_display_ == name)
    return false;

  last_name_to_display_ = name;
  return true;
}

NameForm ProfileAttributesEntry::GetNameForm() const {
  std::u16string name_to_display = GetGAIANameToDisplay();
  if (name_to_display.empty())
    return NameForm::kLocalName;
  if (!ShouldShowProfileLocalName(name_to_display))
    return NameForm::kGaiaName;
  return NameForm::kGaiaAndLocalName;
}

std::u16string ProfileAttributesEntry::GetName() const {
  switch (GetNameForm()) {
    case NameForm::kGaiaName:
      return GetGAIANameToDisplay();
    case NameForm::kLocalName:
      return GetLocalProfileName();
    case NameForm::kGaiaAndLocalName:
      return GetGAIANameToDisplay() + u" (" + GetLocalProfileName() + u")";
  }
}

std::u16string ProfileAttributesEntry::GetShortcutName() const {
  return GetString16(kShortcutNameKey);
}

base::FilePath ProfileAttributesEntry::GetPath() const {
  return profile_path_;
}

base::Time ProfileAttributesEntry::GetActiveTime() const {
  if (IsDouble(kActiveTimeKey)) {
    return base::Time::FromSecondsSinceUnixEpoch(GetDouble(kActiveTimeKey));
  } else {
    return base::Time();
  }
}

std::u16string ProfileAttributesEntry::GetUserName() const {
  return GetString16(kUserNameKey);
}

gfx::Image ProfileAttributesEntry::GetAvatarIcon(
    int size_for_placeholder_avatar,
    bool use_high_res_file,
    const PlaceholderAvatarIconParams& icon_params) const {
  if (IsUsingGAIAPicture()) {
    const gfx::Image* image = GetGAIAPicture();
    if (image)
      return *image;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40138086): After launch, remove the treatment of placeholder
  // avatars from GetHighResAvatar() and from any other places.
  if (GetAvatarIconIndex() == profiles::GetPlaceholderAvatarIndex()) {
    return GetPlaceholderAvatarIcon(size_for_placeholder_avatar, icon_params);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  // Use the high resolution version of the avatar if it exists. Mobile doesn't
  // need the high resolution version so no need to fetch it.
  if (use_high_res_file) {
    const gfx::Image* image = GetHighResAvatar();
    if (image)
      return *image;
  }
#endif

  const int icon_index = GetAvatarIconIndex();
#if BUILDFLAG(IS_WIN)
  if (!profiles::IsModernAvatarIconIndex(icon_index)) {
    // Return the 2x version of the old avatar, defined specifically for
    // Windows. No special treatment is needed for modern avatars as they
    // already have high enough resolution.
    const int win_resource_id =
        profiles::GetOldDefaultAvatar2xIconResourceIDAtIndex(icon_index);
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        win_resource_id);
  }
#endif
  int resource_id = profiles::GetDefaultAvatarIconResourceIDAtIndex(icon_index);
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

bool ProfileAttributesEntry::GetBackgroundStatus() const {
  return GetBool(kBackgroundAppsKey);
}

std::u16string ProfileAttributesEntry::GetGAIAName() const {
  std::u16string gaia_name = GetString16(kGAIANameKey);
  return gaia_name.empty() ? GetString16(kOIDCIdentityNameKey) : gaia_name;
}

std::u16string ProfileAttributesEntry::GetGAIAGivenName() const {
  return GetString16(kGAIAGivenNameKey);
}

std::string ProfileAttributesEntry::GetGAIAId() const {
  return GetString(ProfileAttributesEntry::kGAIAIdKey);
}

const gfx::Image* ProfileAttributesEntry::GetGAIAPicture() const {
  std::string file_name = GetString(kGAIAPictureFileNameKey);

  // If the picture is not on disk then return nullptr.
  if (file_name.empty())
    return nullptr;

  base::FilePath image_path = profile_path_.AppendASCII(file_name);
  return profile_attributes_storage_->LoadAvatarPictureFromPath(
      profile_path_, storage_key_, image_path);
}

bool ProfileAttributesEntry::IsUsingGAIAPicture() const {
  bool result = GetBool(kUseGAIAPictureKey);
  if (!result) {
    // Prefer the GAIA avatar over a non-customized avatar.
    result = IsUsingDefaultAvatar() && GetGAIAPicture();
  }
  return result;
}

bool ProfileAttributesEntry::IsGAIAPictureLoaded() const {
  return profile_attributes_storage_->IsGAIAPictureLoaded(storage_key_);
}

std::string ProfileAttributesEntry::GetLastDownloadedGAIAPictureUrlWithSize()
    const {
  return GetString(kLastDownloadedGAIAPictureUrlWithSizeKey);
}

bool ProfileAttributesEntry::IsSupervised() const {
  return !GetSupervisedUserId().empty();
}

bool ProfileAttributesEntry::IsOmitted() const {
  return is_omitted_;
}

bool ProfileAttributesEntry::IsSigninRequired() const {
  return GetBool(kForceSigninProfileLockedKey);
}

std::string ProfileAttributesEntry::GetSupervisedUserId() const {
  return GetString(kSupervisedUserId);
}

bool ProfileAttributesEntry::IsEphemeral() const {
  return GetBool(kProfileIsEphemeral);
}

bool ProfileAttributesEntry::IsUsingDefaultName() const {
  return GetBool(kIsUsingDefaultNameKey);
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

bool ProfileAttributesEntry::CanBeManaged() const {
  switch (GetSigninState()) {
    case SigninState::kSignedInWithConsentedPrimaryAccount:
      return true;
    case SigninState::kSignedInWithUnconsentedPrimaryAccount:
      return GetBool(kUserAcceptedAccountManagement);
    case SigninState::kNotSignedIn:
      return false;
  }
}

bool ProfileAttributesEntry::IsUsingDefaultAvatar() const {
  return GetBool(kIsUsingDefaultAvatarKey);
}

bool ProfileAttributesEntry::IsSignedInWithCredentialProvider() const {
  return GetBool(prefs::kSignedInWithCredentialProvider);
}

bool ProfileAttributesEntry::IsDasherlessManagement() const {
  return GetBool(kDasherlessManagement);
}

size_t ProfileAttributesEntry::GetAvatarIconIndex() const {
  std::string icon_url = GetString(kAvatarIconKey);
  size_t icon_index = 0;
  if (!profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index))
    DLOG(WARNING) << "Unknown avatar icon: " << icon_url;

  return icon_index;
}

std::optional<ProfileThemeColors>
ProfileAttributesEntry::GetProfileThemeColorsIfSet() const {
  std::optional<SkColor> profile_highlight_color =
      GetProfileThemeColor(kProfileHighlightColorKey);
  std::optional<SkColor> default_avatar_fill_color =
      GetProfileThemeColor(kDefaultAvatarFillColorKey);
  std::optional<SkColor> default_avatar_stroke_color =
      GetProfileThemeColor(kDefaultAvatarStrokeColorKey);
  std::optional<SkColor> profile_color_seed =
      GetProfileThemeColor(kProfileColorSeedKey);

  DCHECK_EQ(profile_highlight_color.has_value(),
            default_avatar_stroke_color.has_value());
  DCHECK_EQ(profile_highlight_color.has_value(),
            default_avatar_fill_color.has_value());

  if (!profile_highlight_color.has_value()) {
    return std::nullopt;
  }

  ProfileThemeColors colors;
  colors.profile_highlight_color = profile_highlight_color.value();
  colors.default_avatar_fill_color = default_avatar_fill_color.value();
  colors.default_avatar_stroke_color = default_avatar_stroke_color.value();
  colors.profile_color_seed =
      profile_color_seed.value_or(profile_highlight_color.value());

  return colors;
}

ProfileThemeColors ProfileAttributesEntry::GetProfileThemeColors() const {
#if BUILDFLAG(IS_ANDROID)
  // Profile theme colors shouldn't be queried on Android.
  NOTREACHED_IN_MIGRATION();
  return {gfx::kPlaceholderColor, gfx::kPlaceholderColor,
          gfx::kPlaceholderColor, gfx::kPlaceholderColor};
#else
  std::optional<ProfileThemeColors> theme_colors = GetProfileThemeColorsIfSet();
  if (theme_colors)
    return *theme_colors;

  return GetDefaultProfileThemeColors();
#endif
}

size_t ProfileAttributesEntry::GetMetricsBucketIndex() {
  int bucket_index = GetInteger(kMetricsBucketIndex);
  if (bucket_index == kIntegerNotSet) {
    bucket_index = NextAvailableMetricsBucketIndex();
    SetInteger(kMetricsBucketIndex, bucket_index);
  }
  return bucket_index;
}

std::string ProfileAttributesEntry::GetHostedDomain() const {
  return GetString(kHostedDomain);
}

std::string ProfileAttributesEntry::GetProfileManagementEnrollmentToken()
    const {
  return GetString(kProfileManagementEnrollmentToken);
}

ProfileManagementOidcTokens
ProfileAttributesEntry::GetProfileManagementOidcTokens() const {
  return ProfileManagementOidcTokens(GetString(kProfileManagementOidcAuthToken),
                                     GetString(kProfileManagementOidcIdToken),
                                     GetString(kProfileManagementOidcState));
}

std::string ProfileAttributesEntry::GetProfileManagementId() const {
  return GetString(kProfileManagementId);
}

std::string ProfileAttributesEntry::GetAccountIdKey() const {
  return GetString(kAccountIdKey);
}

base::flat_set<std::string> ProfileAttributesEntry::GetGaiaIds() const {
  const base::Value* accounts = GetValue(kAllAccountsKey);
  if (!accounts || !accounts->is_dict())
    return base::flat_set<std::string>();

  return base::MakeFlatSet<std::string>(
      accounts->GetDict(), {}, [](const auto& it) { return it.first; });
}

void ProfileAttributesEntry::SetGaiaIds(
    const base::flat_set<std::string>& gaia_ids) {
  base::Value::Dict accounts;
  for (const auto& gaia_id : gaia_ids) {
    // The dictionary is empty for now, but can hold account-specific info in
    // the future.
    accounts.Set(gaia_id, base::Value::Dict());
  }
  SetValue(kAllAccountsKey, base::Value(std::move(accounts)));
}

void ProfileAttributesEntry::SetLocalProfileName(const std::u16string& name,
                                                 bool is_default_name) {
  bool changed = SetString16(kNameKey, name);
  changed |= SetBool(kIsUsingDefaultNameKey, is_default_name);
  if (changed)
    profile_attributes_storage_->NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesEntry::SetShortcutName(const std::u16string& name) {
  SetString16(kShortcutNameKey, name);
}

void ProfileAttributesEntry::SetActiveTimeToNow() {
  if (IsDouble(kActiveTimeKey) &&
      base::Time::Now() - GetActiveTime() < base::Hours(1)) {
    return;
  }
  SetDouble(kActiveTimeKey, base::Time::Now().InSecondsFSinceUnixEpoch());
}

void ProfileAttributesEntry::SetIsOmitted(bool is_omitted) {
  bool old_value = IsOmitted();
  SetIsOmittedInternal(is_omitted);

  // Send a notification only if the value has really changed.
  if (old_value != is_omitted_)
    profile_attributes_storage_->NotifyProfileIsOmittedChanged(GetPath());
}

void ProfileAttributesEntry::SetSupervisedUserId(const std::string& id) {
  if (SetString(kSupervisedUserId, id))
    profile_attributes_storage_->NotifyProfileSupervisedUserIdChanged(
        GetPath());
}

void ProfileAttributesEntry::SetBackgroundStatus(bool running_background_apps) {
  SetBool(kBackgroundAppsKey, running_background_apps);
}

void ProfileAttributesEntry::SetGAIAName(const std::u16string& name) {
  if (SetString16(kGAIANameKey, name))
    profile_attributes_storage_->NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesEntry::SetGAIAGivenName(const std::u16string& name) {
  if (SetString16(kGAIAGivenNameKey, name))
    profile_attributes_storage_->NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesEntry::SetGAIAPicture(
    const std::string& image_url_with_size,
    gfx::Image image) {
  if (!ShouldUpdateGAIAPicture(image_url_with_size, image.IsEmpty()))
    return;

  std::string old_file_name = GetString(kGAIAPictureFileNameKey);
  std::string new_file_name;
  if (image.IsEmpty()) {
    // Delete the old bitmap from disk.
    base::FilePath image_path = profile_path_.AppendASCII(old_file_name);
    profile_attributes_storage_->DeleteGAIAImageAtPath(
        profile_path_, storage_key_, image_path);
  } else {
    // Save the new bitmap to disk.
    new_file_name =
        old_file_name.empty()
            ? base::FilePath(profiles::kGAIAPictureFileName).MaybeAsASCII()
            : old_file_name;
    base::FilePath image_path = profile_path_.AppendASCII(new_file_name);
    profile_attributes_storage_->SaveGAIAImageAtPath(
        profile_path_, storage_key_, image, image_path, image_url_with_size);
  }

  SetString(kGAIAPictureFileNameKey, new_file_name);
  profile_attributes_storage_->NotifyOnProfileAvatarChanged(profile_path_);
}

void ProfileAttributesEntry::SetIsUsingGAIAPicture(bool value) {
  if (SetBool(kUseGAIAPictureKey, value)) {
    profile_attributes_storage_->NotifyOnProfileAvatarChanged(profile_path_);
  }
}

void ProfileAttributesEntry::SetLastDownloadedGAIAPictureUrlWithSize(
    const std::string& image_url_with_size) {
  SetString(kLastDownloadedGAIAPictureUrlWithSizeKey, image_url_with_size);
}

void ProfileAttributesEntry::SetSignedInWithCredentialProvider(bool value) {
  SetBool(prefs::kSignedInWithCredentialProvider, value);
}

void ProfileAttributesEntry::SetDasherlessManagement(bool value) {
  SetBool(kDasherlessManagement, value);
}

void ProfileAttributesEntry::LockForceSigninProfile(bool is_lock) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (SetBool(kForceSigninProfileLockedKey, is_lock)) {
    profile_attributes_storage_->NotifyIsSigninRequiredChanged(GetPath());
  }
}

void ProfileAttributesEntry::SetIsEphemeral(bool value) {
  if (!value) {
    DCHECK(!IsOmitted()) << "An omitted account should not be made "
                            "non-ephemeral. Call SetIsOmitted(false) first.";
  }

  SetBool(kProfileIsEphemeral, value);
}

void ProfileAttributesEntry::SetUserAcceptedAccountManagement(bool value) {
  if (SetBool(kUserAcceptedAccountManagement, value))
    profile_attributes_storage_->NotifyProfileUserManagementAcceptanceChanged(
        GetPath());
}

bool ProfileAttributesEntry::UserAcceptedAccountManagement() const {
  return GetBool(kUserAcceptedAccountManagement);
}

void ProfileAttributesEntry::SetIsUsingDefaultAvatar(bool value) {
  SetBool(kIsUsingDefaultAvatarKey, value);
}

void ProfileAttributesEntry::SetAvatarIconIndex(size_t icon_index) {
  std::string default_avatar_icon_url =
      profiles::GetDefaultAvatarIconUrl(icon_index);
  if (SetString(kAvatarIconKey, default_avatar_icon_url)) {
    // On Windows, Taskbar and Desktop icons are refreshed every time
    // |OnProfileAvatarChanged| notification is fired.
    // As the current avatar icon is already set to |default_avatar_icon_url|,
    // it is important to avoid firing |OnProfileAvatarChanged| in this case.
    // See http://crbug.com/900374
    base::FilePath profile_path = GetPath();
    if (!profile_attributes_storage_->GetDisableAvatarDownloadForTesting()) {
      profile_attributes_storage_->DownloadHighResAvatarIfNeeded(icon_index,
                                                                 profile_path);
    }

    profile_attributes_storage_->NotifyOnProfileAvatarChanged(profile_path);
  }
}

void ProfileAttributesEntry::SetProfileThemeColors(
    const std::optional<ProfileThemeColors>& colors) {
  bool changed = false;
  if (colors.has_value()) {
    changed |=
        SetInteger(kProfileHighlightColorKey, colors->profile_highlight_color);
    changed |= SetInteger(kDefaultAvatarFillColorKey,
                          colors->default_avatar_fill_color);
    changed |= SetInteger(kDefaultAvatarStrokeColorKey,
                          colors->default_avatar_stroke_color);
    changed |= SetInteger(kProfileColorSeedKey, colors->profile_color_seed);
  } else {
    changed |= ClearValue(kProfileHighlightColorKey);
    changed |= ClearValue(kDefaultAvatarFillColorKey);
    changed |= ClearValue(kDefaultAvatarStrokeColorKey);
    changed |= ClearValue(kProfileColorSeedKey);
  }

  if (changed) {
    profile_attributes_storage_->NotifyProfileThemeColorsChanged(GetPath());
  }

  // If the kOutlineSilhouetteIcon feature state has changed, notify that the
  // avatar icon has changed once so that cached avatar images will be updated
  // (e.g. the application badge icon on Windows).
  if (base::FeatureList::IsEnabled(kOutlineSilhouetteIcon) !=
      GetBool(kIsUsingNewPlaceholderAvatarIcon)) {
    SetBool(kIsUsingNewPlaceholderAvatarIcon,
            base::FeatureList::IsEnabled(kOutlineSilhouetteIcon));
    changed = true;
  }

  // Only notify if the profile uses the placeholder avatar.
  if (changed &&
      GetAvatarIconIndex() == profiles::GetPlaceholderAvatarIndex()) {
    profile_attributes_storage_->NotifyOnProfileAvatarChanged(GetPath());
  }
}

void ProfileAttributesEntry::SetHostedDomain(std::string hosted_domain) {
  if (SetString(kHostedDomain, hosted_domain))
    profile_attributes_storage_->NotifyProfileHostedDomainChanged(GetPath());
}

void ProfileAttributesEntry::SetProfileManagementEnrollmentToken(
    const std::string& enrollment_token) {
  if (SetString(kProfileManagementEnrollmentToken, enrollment_token)) {
    profile_attributes_storage_->NotifyProfileManagementEnrollmentTokenChanged(
        GetPath());
  }
}

void ProfileAttributesEntry::SetProfileManagementOidcTokens(
    const ProfileManagementOidcTokens& oidc_tokens) {
  SetString(kProfileManagementOidcAuthToken, oidc_tokens.auth_token);
  SetString(kProfileManagementOidcIdToken, oidc_tokens.id_token);
  SetString(kProfileManagementOidcState, oidc_tokens.state);
  if (SetString16(kOIDCIdentityNameKey, oidc_tokens.identity_name)) {
    profile_attributes_storage_->NotifyIfProfileNamesHaveChanged();
  }
}

void ProfileAttributesEntry::SetProfileManagementId(const std::string& id) {
  if (SetString(kProfileManagementId, id)) {
    profile_attributes_storage_->NotifyProfileManagementIdChanged(GetPath());
  }
}

void ProfileAttributesEntry::SetAuthInfo(const std::string& gaia_id,
                                         const std::u16string& user_name,
                                         bool is_consented_primary_account) {
  // If gaia_id, username and consent state are unchanged, abort early.
  if (GetBool(kIsConsentedPrimaryAccountKey) == is_consented_primary_account &&
      gaia_id == GetGAIAId() && user_name == GetUserName()) {
    return;
  }

  {
    // Bundle the changes in a single update.
    ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
    base::Value::Dict& attributes_dict = update.Get();
    base::Value::Dict* entry = attributes_dict.EnsureDict(storage_key_);
    entry->Set(kGAIAIdKey, gaia_id);
    entry->Set(kUserNameKey, user_name);
    DCHECK(!is_consented_primary_account || !gaia_id.empty() ||
           !user_name.empty());
    entry->Set(kIsConsentedPrimaryAccountKey, is_consented_primary_account);
  }

  profile_attributes_storage_->NotifyProfileAuthInfoChanged(profile_path_);
}

void ProfileAttributesEntry::AddAccountName(const std::string& name) {
  int hash = GetLowEntropyHashValue(name);
  int first_hash = GetInteger(kFirstAccountNameHash);
  if (first_hash == kIntegerNotSet) {
    SetInteger(kFirstAccountNameHash, hash);
    return;
  }

  if (first_hash != hash) {
    SetBool(kHasMultipleAccountNames, true);
  }
}

void ProfileAttributesEntry::ClearAccountNames() {
  ClearValue(kFirstAccountNameHash);
  ClearValue(kHasMultipleAccountNames);
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
  return profile_attributes_storage_->LoadAvatarPictureFromPath(GetPath(), key,
                                                                image_path);
}

gfx::Image ProfileAttributesEntry::GetPlaceholderAvatarIcon(
    int size,
    const PlaceholderAvatarIconParams& icon_params) const {
  ProfileThemeColors colors = GetProfileThemeColors();

  // Filled Person Icon
  if (!base::FeatureList::IsEnabled(kOutlineSilhouetteIcon)) {
    return profiles::GetPlaceholderAvatarIconWithColors(
        colors.default_avatar_fill_color, colors.default_avatar_stroke_color,
        size, icon_params);
  }

  // Outline Silhouette Person Icon
  if (icon_params.visibility_against_background.has_value()) {
    // If the icon should be visible against the background, it cannot have a
    // background or padding.
    CHECK(!icon_params.has_background);
    CHECK(!icon_params.has_padding);
    return profiles::GetPlaceholderAvatarIconVisibleAgainstBackground(
        colors.profile_color_seed, size,
        icon_params.visibility_against_background.value());
  }

  return profiles::GetPlaceholderAvatarIconWithColors(
      colors.default_avatar_fill_color, colors.default_avatar_stroke_color,
      size, icon_params);
}

bool ProfileAttributesEntry::HasMultipleAccountNames() const {
  // If the value is not set, GetBool() returns false.
  return GetBool(kHasMultipleAccountNames);
}

void ProfileAttributesEntry::RecordAccountNamesMetric() const {
  if (HasMultipleAccountNames()) {
    profile_metrics::LogProfileAllAccountsNames(
        IsAuthenticated()
            ? profile_metrics::AllAccountsNames::kMultipleNamesWithSync
            : profile_metrics::AllAccountsNames::kMultipleNamesWithoutSync);
  } else {
    profile_metrics::LogProfileAllAccountsNames(
        profile_metrics::AllAccountsNames::kLikelySingleName);
  }
}

const base::Value::Dict* ProfileAttributesEntry::GetEntryData() const {
  if (!prefs_) {
    return nullptr;
  }

  const base::Value::Dict& attributes =
      prefs_->GetDict(prefs::kProfileAttributes);
  return attributes.FindDict(storage_key_);
}

const base::Value* ProfileAttributesEntry::GetValue(const char* key) const {
  const base::Value::Dict* entry_data = GetEntryData();
  return entry_data ? entry_data->Find(key) : nullptr;
}

std::string ProfileAttributesEntry::GetString(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_string())
    return std::string();
  return value->GetString();
}

std::u16string ProfileAttributesEntry::GetString16(const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_string())
    return std::u16string();
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
    return kIntegerNotSet;
  return value->GetInt();
}

std::optional<SkColor> ProfileAttributesEntry::GetProfileThemeColor(
    const char* key) const {
  // Do not use GetInteger(), as it defaults to kIntegerNotSet which is
  // undistinguishable from a valid color.
  const base::Value* value = GetValue(key);
  if (!value || !value->is_int())
    return std::nullopt;
  return value->GetInt();
}

// Type checking. Only IsDouble is implemented because others do not have
// callsites.
bool ProfileAttributesEntry::IsDouble(const char* key) const {
  const base::Value* value = GetValue(key);
  return value && value->is_double();
}

// Internal setters using keys;
bool ProfileAttributesEntry::SetString(const char* key,
                                       const std::string& value) {
  std::string old_value = GetString(key);
  return SetValue(key, base::Value(value)) && old_value != value;
}

bool ProfileAttributesEntry::SetString16(const char* key,
                                         const std::u16string& value) {
  std::u16string old_value = GetString16(key);
  return SetValue(key, base::Value(value)) && old_value != value;
}

bool ProfileAttributesEntry::SetDouble(const char* key, double value) {
  double old_value = GetDouble(key);
  return SetValue(key, base::Value(value)) && old_value != value;
}

bool ProfileAttributesEntry::SetBool(const char* key, bool value) {
  bool old_value = GetBool(key);
  return SetValue(key, base::Value(value)) && old_value != value;
}

bool ProfileAttributesEntry::SetInteger(const char* key, int value) {
  int old_value = GetInteger(key);
  return SetValue(key, base::Value(value)) && old_value != value;
}

bool ProfileAttributesEntry::SetValue(const char* key, base::Value value) {
  const base::Value* old_value = GetValue(key);
  if (old_value && *old_value == value)
    return false;

  ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
  base::Value::Dict& attributes_dict = update.Get();
  base::Value::Dict* entry = attributes_dict.EnsureDict(storage_key_);
  entry->Set(key, std::move(value));
  return true;
}

bool ProfileAttributesEntry::ClearValue(const char* key) {
  const base::Value* old_value = GetValue(key);
  if (!old_value)
    return false;

  ScopedDictPrefUpdate update(prefs_, prefs::kProfileAttributes);
  base::Value::Dict& attributes_dict = update.Get();
  base::Value::Dict* entry = attributes_dict.FindDict(storage_key_);
  DCHECK(entry);
  entry->Remove(key);
  return true;
}

// This method should be periodically pruned of year+ old migrations.
void ProfileAttributesEntry::MigrateObsoleteProfileAttributes() {
  // Added 3/2023.
  ClearValue(kAccountCategories);
}

void ProfileAttributesEntry::SetIsOmittedInternal(bool is_omitted) {
  if (is_omitted) {
    DCHECK(IsEphemeral()) << "Only ephemeral profiles can be omitted.";
  }

  is_omitted_ = is_omitted;
}
