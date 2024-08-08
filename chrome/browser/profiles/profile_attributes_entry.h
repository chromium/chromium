// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_ENTRY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_ENTRY_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

class PrefRegistrySimple;
class PrefService;
class ProfileAttributesStorage;
struct ProfileThemeColors;

inline constexpr int kDefaultSizeForPlaceholderAvatar = 74;

enum class SigninState {
  kNotSignedIn,
  kSignedInWithUnconsentedPrimaryAccount,
  kSignedInWithConsentedPrimaryAccount,
};

enum class NameForm {
  kGaiaName,
  kLocalName,
  kGaiaAndLocalName,
};

struct ProfileManagementOidcTokens {
  ProfileManagementOidcTokens();
  ProfileManagementOidcTokens(const std::string& auth_token,
                              const std::string& id_token,
                              const std::u16string& identity_name);
  ProfileManagementOidcTokens(const std::string& auth_token,
                              const std::string& id_token,
                              const std::string& state);

  ProfileManagementOidcTokens(const ProfileManagementOidcTokens& other);
  ProfileManagementOidcTokens& operator=(
      const ProfileManagementOidcTokens& other);

  ProfileManagementOidcTokens(ProfileManagementOidcTokens&& other);
  ProfileManagementOidcTokens& operator=(ProfileManagementOidcTokens&& other);
  ~ProfileManagementOidcTokens();

  // Authorization token from the authorization response.
  std::string auth_token;

  // ID token from the authorization response.
  std::string id_token;

  // Identity name of the profile. This is only relevant after the completion of
  // profile registration.
  std::u16string identity_name;

  // OIDC configuration state. This is only relevant during profile
  // registration.
  std::string state;
};

class ProfileAttributesEntry {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  ProfileAttributesEntry();
  ProfileAttributesEntry(const ProfileAttributesEntry&) = delete;
  ProfileAttributesEntry& operator=(const ProfileAttributesEntry&) = delete;
  virtual ~ProfileAttributesEntry() = default;

  // Gets the name of the profile to be displayed in the User Menu. The name can
  // be the GAIA name, local profile name or a combination of them.
  std::u16string GetName() const;
  // Returns |GetGAIAGivenName()| if not empty. Otherwise, returns
  // |GetGAIAName()|.
  std::u16string GetGAIANameToDisplay() const;
  // Returns true if the profile name has changed.
  bool HasProfileNameChanged();
  // Returns how the value of GetName() gets constructed.
  NameForm GetNameForm() const;

  // Gets the local profile name.
  std::u16string GetLocalProfileName() const;

  std::u16string GetShortcutName() const;
  // Gets the path to the profile. Should correspond to the path passed to
  // ProfileAttributesStorage::GetProfileAttributesWithPath to get this entry.
  base::FilePath GetPath() const;
  base::Time GetActiveTime() const;
  // Gets the user name of the signed in profile. This is typically the email
  // address used to sign in and the empty string for profiles that aren't
  // signed in to chrome.
  std::u16string GetUserName() const;
  // Gets the icon used as this profile's avatar. High res icon are downloaded
  // only if `download_high_res` is true, otherwise a low-res fallback is
  // returned.
  // TODO(crbug.com/40138086): Rename |size_for_placeholder_avatar| to |size|
  // and make this function resize all avatars appropriately. Remove the default
  // value of |size_for_placeholder_avatar| when all callsites pass some value.
  // Consider adding a |shape| parameter and get rid of
  // profiles::GetSizedAvatarIcon().
  gfx::Image GetAvatarIcon(
      int size_for_placeholder_avatar = kDefaultSizeForPlaceholderAvatar,
      bool use_high_res_file = true,
      const profiles::PlaceholderAvatarIconParams& icon_params = {}) const;
  // Returns true if the profile is currently running any background apps. Note
  // that a return value of false could mean an error in collection or that
  // there are currently no background apps running. However, the action which
  // results is the same in both cases (thus far).
  bool GetBackgroundStatus() const;
  // Gets the GAIA full name associated with this profile if it's signed in.
  // If GAIA full name is empty, gets the full name from the 3P identity
  // associated with this profile, currently only available for OIDC profiles.
  std::u16string GetGAIAName() const;
  // Gets the GAIA given name associated with this profile if it's signed in.
  std::u16string GetGAIAGivenName() const;
  // Gets the opaque string representation of the profile's GAIA ID if it's
  // signed in.
  std::string GetGAIAId() const;
  // Returns the GAIA picture for the given profile. This may return NULL
  // if the profile does not have a GAIA picture or if the picture must be
  // loaded from disk.
  const gfx::Image* GetGAIAPicture() const;
  // Returns true if the profile displays a GAIA picture instead of one of the
  // locally bundled icons.
  bool IsUsingGAIAPicture() const;
  // Returns true if a GAIA picture has been loaded or has failed to load.
  bool IsGAIAPictureLoaded() const;
  // Returns the last downloaded GAIA picture URL with size.
  std::string GetLastDownloadedGAIAPictureUrlWithSize() const;
  // Returns true if the profile is signed in as a supervised user.
  bool IsSupervised() const;
  // Returns true if the profile should not be displayed to the user in the
  // list of profiles.
  bool IsOmitted() const;
  // Returns true if the user must sign before a profile can be opened.
  // Currently, this returns true iff a profile is locked due to the force
  // sign-in policy.
  bool IsSigninRequired() const;
  // Gets the supervised user ID of the profile's signed in account, if it's a
  // supervised user.
  std::string GetSupervisedUserId() const;
  // Returns true if the profile is an ephemeral profile.
  bool IsEphemeral() const;
  // Returns true if the profile is using a default name, typically of the
  // format "Person %d".
  bool IsUsingDefaultName() const;
  // Returns Signin state.
  SigninState GetSigninState() const;
  // Returns true if the profile is signed in.
  bool IsAuthenticated() const;
  // Returns true if the account can be be managed.
  bool CanBeManaged() const;
  // Returns true if the Profile is using the default avatar, which is one of
  // the profile icons selectable at profile creation.
  bool IsUsingDefaultAvatar() const;
  // Indicates that profile was signed in through native OS credential provider.
  bool IsSignedInWithCredentialProvider() const;
  // Returns true if the profile is managed by a third party identity that is
  // not sync-ed to Google (i.e dasher-based).
  bool IsDasherlessManagement() const;
  // Returns the index of the default icon used by the profile.
  size_t GetAvatarIconIndex() const;
  // Returns the colors specified by the profile theme, or default colors if no
  // theme is specified for the profile.
  ProfileThemeColors GetProfileThemeColors() const;
  // Returns the colors specified by the profile theme, or empty if no theme is
  // set for the profile.
  std::optional<ProfileThemeColors> GetProfileThemeColorsIfSet() const;
  // Returns the metrics bucket this profile should be recorded in.
  // Note: The bucket index is assigned once and remains the same all time. 0 is
  // reserved for the guest profile.
  size_t GetMetricsBucketIndex();
  // Returns the hosted domain for the current signed-in account. Returns empty
  // string if there is no signed-in account and returns |kNoHostedDomainFound|
  // if the signed-in account has no hosted domain (such as when it is a
  // standard gmail.com account). Unlike for other string getters, the returned
  // value is UTF8 encoded.
  std::string GetHostedDomain() const;

  // Returns the enrollment token to get policies for a profile.
  std::string GetProfileManagementEnrollmentToken() const;

  // Returns the Oauth token and Id token from the OIDC authentication response
  // that created the profile. The existence of these tokens are also used to
  // check whether the profile is created by an OIDC authentication response.
  ProfileManagementOidcTokens GetProfileManagementOidcTokens() const;

  // Returns the signin id for a profile managed by a token. This may be empty
  // even if there is an enrollment token.
  std::string GetProfileManagementId() const;

  // Returns an account id key of the user of the profile. Empty if the profile
  // doesn't have any associated `user_manager::User`.
  std::string GetAccountIdKey() const;

  // Gets/Sets the gaia IDs of the accounts signed into the profile (accounts
  // known by the `IdentityManager`).
  base::flat_set<std::string> GetGaiaIds() const;
  void SetGaiaIds(const base::flat_set<std::string>& gaia_ids);

  // |is_using_default| should be set to false for non default profile names.
  void SetLocalProfileName(const std::u16string& name, bool is_default_name);
  void SetShortcutName(const std::u16string& name);
  void SetActiveTimeToNow();
  // Only ephemeral profiles can be set as omitted.
  void SetIsOmitted(bool is_omitted);
  void SetSupervisedUserId(const std::string& id);
  void SetBackgroundStatus(bool running_background_apps);
  void SetGAIAName(const std::u16string& name);
  void SetGAIAGivenName(const std::u16string& name);
  void SetGAIAPicture(const std::string& image_url_with_size, gfx::Image image);
  void SetIsUsingGAIAPicture(bool value);
  void SetLastDownloadedGAIAPictureUrlWithSize(
      const std::string& image_url_with_size);
  void SetSignedInWithCredentialProvider(bool value);
  void SetDasherlessManagement(bool value);
  // Only non-omitted profiles can be set as non-ephemeral. It's the
  // responsibility of the caller to make sure that the entry is set as
  // non-ephemeral only if prefs::kForceEphemeralProfiles is false.
  void SetIsEphemeral(bool value);
  void SetUserAcceptedAccountManagement(bool value);
  bool UserAcceptedAccountManagement() const;
  void SetIsUsingDefaultAvatar(bool value);
  void SetAvatarIconIndex(size_t icon_index);
  // std::nullopt resets colors to default.
  void SetProfileThemeColors(const std::optional<ProfileThemeColors>& colors);

  // Unlike for other string setters, the argument is expected to be UTF8
  // encoded.
  void SetHostedDomain(std::string hosted_domain);

  void SetProfileManagementEnrollmentToken(const std::string& enrollment_token);
  void SetProfileManagementOidcTokens(
      const ProfileManagementOidcTokens& oidc_tokens);
  void SetProfileManagementId(const std::string& id);

  void SetAuthInfo(const std::string& gaia_id,
                   const std::u16string& user_name,
                   bool is_consented_primary_account);

  // Update info about accounts. These functions are idempotent, only the first
  // call for a given input matters.
  void AddAccountName(const std::string& name);

  // Clears info about all accounts that have been added in the past via
  // AddAccountName().
  void ClearAccountNames();

  // Lock/Unlock the profile, should be called only if force-sign-in is enabled.
  void LockForceSigninProfile(bool is_lock);

  // Records aggregate metrics about all accounts used in this profile.
  void RecordAccountNamesMetric() const;

  static const char kSupervisedUserId[];
  static const char kAvatarIconKey[];
  static const char kBackgroundAppsKey[];
  static const char kProfileIsEphemeral[];
  static const char kUserNameKey[];
  static const char kGAIAIdKey[];
  static const char kIsConsentedPrimaryAccountKey[];
  static const char kNameKey[];
  static const char kIsUsingDefaultNameKey[];
  static const char kIsUsingDefaultAvatarKey[];
  static const char kUseGAIAPictureKey[];
  static const char kAccountIdKey[];

 private:
  friend class ProfileAttributesStorage;
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           EntryInternalAccessors);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest, ProfileActiveTime);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           DownloadHighResAvatarTest);

  // Initializes the current entry instance. The callers must subsequently call
  // InitializeLastNameToDisplay() for this entry.
  void Initialize(ProfileAttributesStorage* storage,
                  const base::FilePath& path,
                  PrefService* prefs);

  // Sets the initial name of the profile to be displayed. The name might depend
  // on other's profiles names so this must be called only after all profiles
  // has been initialized.
  void InitializeLastNameToDisplay();
  std::u16string GetLastNameToDisplay() const;

  // Returns true if:
  // - The user has chosen a local profile name on purpose. One exception where
  //   we don't show the local profile name, is when it is equal to the
  //   GAIA name.
  // - If two profiles have the same GAIA name and we need to show the local
  //   profile name to clear ambiguity.
  bool ShouldShowProfileLocalName(
      const std::u16string& gaia_name_to_display) const;

  // Returns true if the current GAIA picture should be updated with an image
  // having provided parameters. `image_is_empty` is true when attempting to
  // clear the current GAIA picture.
  bool ShouldUpdateGAIAPicture(const std::string& image_url_with_size,
                               bool image_is_empty) const;

  // Loads or uses an already loaded high resolution image of the generic
  // profile avatar.
  const gfx::Image* GetHighResAvatar() const;

  // Generates the colored placeholder avatar icon for the given |size|.
  gfx::Image GetPlaceholderAvatarIcon(
      int size,
      const profiles::PlaceholderAvatarIconParams& icon_params) const;

  // Returns if this profile has accounts (signed-in or signed-out) with
  // different account names. This is approximate as only a short hash of an
  // account name is stored so there can be false negatives.
  bool HasMultipleAccountNames() const;

  // Loads and saves the data to the local state.
  const base::Value::Dict* GetEntryData() const;

  // Internal getter that returns a base::Value*, or nullptr if the key is not
  // present.
  const base::Value* GetValue(const char* key) const;

  // Internal getters that return basic data types. If the key is not present,
  // or if the data is in a wrong data type, return empty string, 0.0, false or
  // -1 depending on the target data type. We do not assume that the data type
  // is correct because the local state file can be modified by a third party.
  std::string GetString(const char* key) const;
  std::u16string GetString16(const char* key) const;
  double GetDouble(const char* key) const;
  bool GetBool(const char* key) const;
  int GetInteger(const char* key) const;

  // Internal getter that returns one of the profile theme colors or
  // std::nullopt if the key is not present.
  std::optional<SkColor> GetProfileThemeColor(const char* key) const;

  // Type checking. Only IsDouble is implemented because others do not have
  // callsites.
  bool IsDouble(const char* key) const;

  // Internal setters that accept basic data types. Return if the original data
  // is different from the new data, i.e. whether actual update is done.
  // If the data was missing or was from a different type and `value` is the
  // default value (e.g. false, 0, empty string...), the value is explicitly
  // written but these return false.
  bool SetString(const char* key, const std::string& value);
  bool SetString16(const char* key, const std::u16string& value);
  bool SetDouble(const char* key, double value);
  bool SetBool(const char* key, bool value);
  bool SetInteger(const char* key, int value);

  // Generic setter, used to implement the more specific ones. If the value was
  // missing and `value` is the default value (e.g. false, 0, empty string...),
  // the value is written and this returns true.
  bool SetValue(const char* key, base::Value value);

  // Clears value stored for |key|. Returns if the original data is different
  // from the new data, i.e. whether actual update is done.
  bool ClearValue(const char* key);

  // Migrate/cleanup deprecated keys in profile attributes. Over time, long
  // deprecated keys should be removed as new ones are added, but this call
  // should never go away (even if it becomes an empty call for some time) as it
  // should remain *the* place to drop deprecated profile attributes keys at.
  void MigrateObsoleteProfileAttributes();

  // Internal version of `SetIsOmitted()` that doesn't trigger any
  // notifications.
  void SetIsOmittedInternal(bool is_omitted);

  raw_ptr<ProfileAttributesStorage> profile_attributes_storage_ = nullptr;
  raw_ptr<PrefService> prefs_ = nullptr;
  base::FilePath profile_path_;
  std::string storage_key_;
  std::u16string last_name_to_display_;

  // Indicates whether the profile should not be displayed to the user in the
  // list of profiles. This flag is intended to work only with ephemeral
  // profiles which get removed after the browser restart. Thus, this flag is
  // stored in memory only. Storing in memory also allows to avoid the risk of
  // having permanent profiles that the user cannot see or delete, in case the
  // ephemeral profile deletion fails.
  bool is_omitted_ = false;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_ENTRY_H_
