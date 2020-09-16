// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/state.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/themes/theme_properties.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/ui/signin/profile_colors_util.h"
#endif

namespace {

const char kGAIAGivenNameKey[] = "gaia_given_name";
const char kGAIANameKey[] = "gaia_name";
const char kShortcutNameKey[] = "shortcut_name";
const char kActiveTimeKey[] = "active_time";
const char kAuthCredentialsKey[] = "local_auth_credentials";
const char kPasswordTokenKey[] = "gaia_password_token";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kMetricsBucketIndex[] = "metrics_bucket_index";
const char kSigninRequiredKey[] = "signin_required";
const char kHostedDomain[] = "hosted_domain";

// Profile colors info.
const char kProfileHighlightColorKey[] = "profile_highlight_color";
const char kDefaultAvatarFillColorKey[] = "default_avatar_fill_color";
const char kDefaultAvatarStrokeColorKey[] = "default_avatar_stroke_color";

// Low-entropy accounts info, for metrics only.
const char kFirstAccountNameHash[] = "first_account_name_hash";
const char kHasMultipleAccountNames[] = "has_multiple_account_names";
const char kAccountCategories[] = "account_categories";

// Local state pref to keep track of the next available profile bucket.
const char kNextMetricsBucketIndex[] = "profile.metrics.next_bucket_index";

constexpr int kIntegerNotSet = -1;

// Persisted in prefs.
constexpr int kAccountCategoriesConsumerOnly = 0;
constexpr int kAccountCategoriesEnterpriseOnly = 1;
constexpr int kAccountCategoriesBoth = 2;

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

bool ProfileThemeColors::operator==(const ProfileThemeColors& other) const {
  return std::tie(this->profile_highlight_color,
                  this->default_avatar_fill_color,
                  this->default_avatar_stroke_color) ==
         std::tie(other.profile_highlight_color,
                  other.default_avatar_fill_color,
                  other.default_avatar_stroke_color);
}

bool ProfileThemeColors::operator!=(const ProfileThemeColors& other) const {
  return !(*this == other);
}

const char ProfileAttributesEntry::kSupervisedUserId[] = "managed_user_id";
const char ProfileAttributesEntry::kIsOmittedFromProfileListKey[] =
    "is_omitted_from_profile_list";
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

// static
void ProfileAttributesEntry::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Bucket 0 is reserved for the guest profile, so start new bucket indices
  // at 1.
  registry->RegisterIntegerPref(kNextMetricsBucketIndex, 1);
}

ProfileAttributesEntry::ProfileAttributesEntry() = default;

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
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
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
  return GetString16(kNameKey);
}

base::string16 ProfileAttributesEntry::GetGAIANameToDisplay() const {
  base::string16 gaia_given_name = GetGAIAGivenName();
  return gaia_given_name.empty() ? GetGAIAName() : gaia_given_name;
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

NameForm ProfileAttributesEntry::GetNameForm() const {
  base::string16 name_to_display = GetGAIANameToDisplay();
  if (name_to_display.empty())
    return NameForm::kLocalName;
  if (!ShouldShowProfileLocalName(name_to_display))
    return NameForm::kGaiaName;
  return NameForm::kGaiaAndLocalName;
}

base::string16 ProfileAttributesEntry::GetName() const {
  switch (GetNameForm()) {
    case NameForm::kGaiaName:
      return GetGAIANameToDisplay();
    case NameForm::kLocalName:
      return GetLocalProfileName();
    case NameForm::kGaiaAndLocalName:
      return GetGAIANameToDisplay() + base::UTF8ToUTF16(" (") +
             GetLocalProfileName() + base::UTF8ToUTF16(")");
  }
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

gfx::Image ProfileAttributesEntry::GetAvatarIcon(
    int size_for_placeholder_avatar) const {
  if (IsUsingGAIAPicture()) {
    const gfx::Image* image = GetGAIAPicture();
    if (image)
      return *image;
  }

  // TODO(crbug.com/1100835): After launch, remove the treatment of placeholder
  // avatars from GetHighResAvatar() and from any other places.
  if (base::FeatureList::IsEnabled(features::kNewProfilePicker) &&
      GetAvatarIconIndex() == profiles::GetPlaceholderAvatarIndex()) {
    return GetPlaceholderAvatarIcon(size_for_placeholder_avatar);
  }

#if !defined(OS_ANDROID)
  // Use the high resolution version of the avatar if it exists. Mobile doesn't
  // need the high resolution version so no need to fetch it.
  const gfx::Image* image = GetHighResAvatar();
  if (image)
    return *image;
#endif

  const int icon_index = GetAvatarIconIndex();
#if defined(OS_WIN)
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
  return GetString16(kGAIANameKey);
}

base::string16 ProfileAttributesEntry::GetGAIAGivenName() const {
  return GetString16(kGAIAGivenNameKey);
}

std::string ProfileAttributesEntry::GetGAIAId() const {
  return GetString(ProfileAttributesEntry::kGAIAIdKey);
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
  return !GetSupervisedUserId().empty();
}

bool ProfileAttributesEntry::IsChild() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return GetSupervisedUserId() == supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool ProfileAttributesEntry::IsLegacySupervised() const {
  return IsSupervised() && !IsChild();
}

bool ProfileAttributesEntry::IsOmitted() const {
  return GetBool(kIsOmittedFromProfileListKey);
}

bool ProfileAttributesEntry::IsSigninRequired() const {
  return GetBool(kSigninRequiredKey) || is_force_signin_profile_locked_;
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

bool ProfileAttributesEntry::IsUsingDefaultAvatar() const {
  return profile_info_cache_->ProfileIsUsingDefaultAvatarAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsAuthError() const {
  return GetBool(kIsAuthErrorKey);
}

bool ProfileAttributesEntry::IsSignedInWithCredentialProvider() const {
  return GetBool(prefs::kSignedInWithCredentialProvider);
}

size_t ProfileAttributesEntry::GetAvatarIconIndex() const {
  std::string icon_url = GetString(kAvatarIconKey);
  size_t icon_index = 0;
  if (!profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index))
    DLOG(WARNING) << "Unknown avatar icon: " << icon_url;

  return icon_index;
}

ProfileThemeColors ProfileAttributesEntry::GetProfileThemeColors() const {
  base::Optional<SkColor> profile_highlight_color =
      GetProfileThemeColor(kProfileHighlightColorKey);
  base::Optional<SkColor> default_avatar_fill_color =
      GetProfileThemeColor(kDefaultAvatarFillColorKey);
  base::Optional<SkColor> default_avatar_stroke_color =
      GetProfileThemeColor(kDefaultAvatarStrokeColorKey);
  if (!profile_highlight_color.has_value()) {
    DCHECK(!default_avatar_fill_color.has_value() &&
           !default_avatar_stroke_color.has_value());
    return GetDefaultProfileThemeColors(
        ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors());
  }

  DCHECK(default_avatar_fill_color.has_value() &&
         default_avatar_stroke_color.has_value());
  ProfileThemeColors colors;
  colors.profile_highlight_color = profile_highlight_color.value();
  colors.default_avatar_fill_color = default_avatar_fill_color.value();
  colors.default_avatar_stroke_color = default_avatar_stroke_color.value();
  return colors;
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

void ProfileAttributesEntry::SetLocalProfileName(const base::string16& name) {
  if (SetString16(kNameKey, name))
    profile_info_cache_->NotifyIfProfileNamesHaveChanged();
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
  if (SetBool(kIsOmittedFromProfileListKey, is_omitted))
    profile_info_cache_->NotifyProfileIsOmittedChanged(GetPath());
}

void ProfileAttributesEntry::SetSupervisedUserId(const std::string& id) {
  if (SetString(kSupervisedUserId, id))
    profile_info_cache_->NotifyProfileSupervisedUserIdChanged(GetPath());
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
  if (SetString16(kGAIANameKey, name))
    profile_info_cache_->NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesEntry::SetGAIAGivenName(const base::string16& name) {
  if (SetString16(kGAIAGivenNameKey, name))
    profile_info_cache_->NotifyIfProfileNamesHaveChanged();
}

void ProfileAttributesEntry::SetGAIAPicture(
    const std::string& image_url_with_size,
    gfx::Image image) {
  profile_info_cache_->SetGAIAPictureOfProfileAtIndex(
      profile_index(), image_url_with_size, image);
}

void ProfileAttributesEntry::SetIsUsingGAIAPicture(bool value) {
  profile_info_cache_->SetIsUsingGAIAPictureOfProfileAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsSigninRequired(bool value) {
  if (value != GetBool(kSigninRequiredKey)) {
    SetBool(kSigninRequiredKey, value);
    profile_info_cache_->NotifyIsSigninRequiredChanged(GetPath());
  }
  if (is_force_signin_enabled_)
    LockForceSigninProfile(value);
}

void ProfileAttributesEntry::SetSignedInWithCredentialProvider(bool value) {
  if (value != GetBool(prefs::kSignedInWithCredentialProvider)) {
    SetBool(prefs::kSignedInWithCredentialProvider, value);
  }
}

void ProfileAttributesEntry::LockForceSigninProfile(bool is_lock) {
  DCHECK(is_force_signin_enabled_);
  if (is_force_signin_profile_locked_ == is_lock)
    return;
  is_force_signin_profile_locked_ = is_lock;
  profile_info_cache_->NotifyIsSigninRequiredChanged(GetPath());
}

void ProfileAttributesEntry::RecordAccountMetrics() const {
  RecordAccountCategoriesMetric();
  RecordAccountNamesMetric();
}

void ProfileAttributesEntry::SetIsEphemeral(bool value) {
  SetBool(kProfileIsEphemeral, value);
}

void ProfileAttributesEntry::SetIsUsingDefaultName(bool value) {
  if (SetBool(kIsUsingDefaultNameKey, value))
    profile_info_cache_->NotifyIfProfileNamesHaveChanged();
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

void ProfileAttributesEntry::SetProfileThemeColors(
    const base::Optional<ProfileThemeColors>& colors) {
  if (colors.has_value()) {
    SetInteger(kProfileHighlightColorKey, colors->profile_highlight_color);
    SetInteger(kDefaultAvatarFillColorKey, colors->default_avatar_fill_color);
    SetInteger(kDefaultAvatarStrokeColorKey,
               colors->default_avatar_stroke_color);
  } else {
    ClearValue(kProfileHighlightColorKey);
    ClearValue(kDefaultAvatarFillColorKey);
    ClearValue(kDefaultAvatarStrokeColorKey);
  }
}

void ProfileAttributesEntry::SetHostedDomain(std::string hosted_domain) {
  SetString(kHostedDomain, hosted_domain);
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

void ProfileAttributesEntry::AddAccountCategory(AccountCategory category) {
  int current_categories = GetInteger(kAccountCategories);
  if (current_categories == kAccountCategoriesBoth)
    return;

  int new_category = category == AccountCategory::kConsumer
                         ? kAccountCategoriesConsumerOnly
                         : kAccountCategoriesEnterpriseOnly;
  if (current_categories == kIntegerNotSet) {
    SetInteger(kAccountCategories, new_category);
  } else if (current_categories != new_category) {
    SetInteger(kAccountCategories, kAccountCategoriesBoth);
  }
}

void ProfileAttributesEntry::ClearAccountNames() {
  ClearValue(kFirstAccountNameHash);
  ClearValue(kHasMultipleAccountNames);
}

void ProfileAttributesEntry::ClearAccountCategories() {
  ClearValue(kAccountCategories);
}

size_t ProfileAttributesEntry::profile_index() const {
  size_t index = profile_info_cache_->GetIndexOfProfileWithPath(profile_path_);
  DCHECK(index < profile_info_cache_->GetNumberOfProfiles());
  return index;
}

// static
ProfileThemeColors ProfileAttributesEntry::GetDefaultProfileThemeColors(
    bool dark_mode) {
#if defined(OS_ANDROID)
  // Profile theme colors shouldn't be queried on Android.
  NOTREACHED();
  return {SK_ColorRED, SK_ColorRED, SK_ColorRED};
#else
  ProfileThemeColors default_colors;
  default_colors.profile_highlight_color = ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_FRAME_ACTIVE, /*incognito=*/false, dark_mode);
  default_colors.default_avatar_fill_color = ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_FRAME_ACTIVE, /*incognito=*/false, dark_mode);
  default_colors.default_avatar_stroke_color =
      GetAvatarStrokeColor(default_colors.default_avatar_fill_color);
  return default_colors;
#endif
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

gfx::Image ProfileAttributesEntry::GetPlaceholderAvatarIcon(int size) const {
  ProfileThemeColors colors = GetProfileThemeColors();
  return profiles::GetPlaceholderAvatarIconWithColors(
      colors.default_avatar_fill_color, colors.default_avatar_stroke_color,
      size);
}

bool ProfileAttributesEntry::HasMultipleAccountNames() const {
  // If the value is not set, GetBool() returns false.
  return GetBool(kHasMultipleAccountNames);
}

bool ProfileAttributesEntry::HasBothAccountCategories() const {
  // If the value is not set, GetInteger returns kIntegerNotSet which does not
  // equal kAccountTypeBoth.
  return GetInteger(kAccountCategories) == kAccountCategoriesBoth;
}

void ProfileAttributesEntry::RecordAccountCategoriesMetric() const {
  if (HasBothAccountCategories()) {
    if (IsAuthenticated()) {
      bool consumer_syncing = GetHostedDomain() == kNoHostedDomainFound;
      profile_metrics::LogProfileAllAccountsCategories(
          consumer_syncing ? profile_metrics::AllAccountsCategories::
                                 kBothConsumerAndEnterpriseSyncingConsumer
                           : profile_metrics::AllAccountsCategories::
                                 kBothConsumerAndEnterpriseSyncingEnterprise);
    } else {
      profile_metrics::LogProfileAllAccountsCategories(
          profile_metrics::AllAccountsCategories::
              kBothConsumerAndEnterpriseNoSync);
    }
  } else {
    profile_metrics::LogProfileAllAccountsCategories(
        profile_metrics::AllAccountsCategories::kSingleCategory);
  }
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
    return kIntegerNotSet;
  return value->GetInt();
}

base::Optional<SkColor> ProfileAttributesEntry::GetProfileThemeColor(
    const char* key) const {
  const base::Value* value = GetValue(key);
  if (!value || !value->is_int())
    return base::nullopt;
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

bool ProfileAttributesEntry::ClearValue(const char* key) {
  const base::Value* old_data = GetEntryData();
  if (!old_data || !old_data->FindKey(key))
    return false;

  base::Value new_data = GetEntryData()->Clone();
  new_data.RemoveKey(key);
  SetEntryData(std::move(new_data));
  return true;
}
