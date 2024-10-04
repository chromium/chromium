// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class ChromeZoomLevelPrefs;
class ExtensionSpecialStoragePolicy;
class GURL;
class PrefService;
class PrefStore;
class ProfileDestroyer;
class ProfileKey;
class TestingProfile;
class ThemeService;
class InstantService;

namespace base {
class FilePath;
class SequencedTaskRunner;
class Time;
}

namespace content {
class WebUI;
}

namespace policy {
class SchemaRegistryService;
class ProfilePolicyConnector;
class ProfileCloudPolicyManager;
class UserCloudPolicyManager;
class CloudPolicyManager;

#if BUILDFLAG(IS_CHROMEOS)
class UserCloudPolicyManagerAsh;
#endif
}  // namespace policy

namespace network {
class SharedURLLoaderFactory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class ProfileObserver;

// Instead of adding more members to Profile, consider creating a
// KeyedService. See
// http://dev.chromium.org/developers/design-documents/profile-architecture
class Profile : public content::BrowserContext {
 public:
  enum class CreateMode {
    kSynchronous,
    kAsynchronous,
  };

  // Defines an ID to distinguish different off-the-record profiles of a regular
  // profile.
  class OTRProfileID {
   public:
    // ID used by the Incognito and Guest profiles.
    // TODO(crbug.com/40775669): To be replaced with |IncognitoID| if
    // OTR Guest profiles are deprecated.
    static const OTRProfileID PrimaryID();

    // Creates a unique OTR profile id with the given profile id prefix.
    //
    // WARNING:
    // The use of this class to create non-primary OTR profiles in Desktop
    // platforms is restricted exclusively for cases where extensions should not
    // be applicable to run. Please see crbug.com/1098697#c3 for more details.
    static OTRProfileID CreateUnique(const std::string& profile_id_prefix);

    // Creates a unique OTR profile id to be used for DevTools browser contexts.
    static OTRProfileID CreateUniqueForDevTools();

    // Creates a unique OTR profile id to be used for media router.
    static OTRProfileID CreateUniqueForMediaRouter();

#if BUILDFLAG(IS_CHROMEOS)
    // Creates a unique OTR profile id to be used for captive portal signin on
    // ChromeOS.
    static OTRProfileID CreateUniqueForCaptivePortal();
#endif
    // Creates a unique OTR profile id for tests.
    static OTRProfileID CreateUniqueForTesting();

    bool operator==(const OTRProfileID& other) const {
      return profile_id_ == other.profile_id_;
    }

    bool operator!=(const OTRProfileID& other) const {
      return profile_id_ != other.profile_id_;
    }

    bool operator<(const OTRProfileID& other) const {
      return profile_id_ < other.profile_id_;
    }

    bool AllowsBrowserWindows() const;

#if BUILDFLAG(IS_CHROMEOS)
    // Returns true if the OTR Profile was created for captive portal signin.
    bool IsCaptivePortal() const;
#endif

#if BUILDFLAG(IS_ANDROID)
    // Constructs a Java OTRProfileID from the provided C++ OTRProfileID
    base::android::ScopedJavaLocalRef<jobject> ConvertToJavaOTRProfileID(
        JNIEnv* env) const;

    // Constructs a C++ OTRProfileID from the provided Java OTRProfileID
    static OTRProfileID ConvertFromJavaOTRProfileID(
        JNIEnv* env,
        const base::android::JavaRef<jobject>& j_otr_profile_id);

    // Constructs an OTRProfileID based on the string passed in. Should only be
    // called with values previously returned by Serialize().
    static OTRProfileID Deserialize(const std::string& value);

    // Constructs a string that represents OTRProfileID from the provided
    // OTRProfileID.
    // TODO(crbug.com/40162345): Use one serialize function for both java and
    // native side instead of having duplicate code.
    std::string Serialize() const;
#endif

   private:
    friend std::ostream& operator<<(std::ostream& out,
                                    const OTRProfileID& profile_id);

    // Creates an OTR profile ID from |profile_id|.
    // |profile_id| should follow the following naming scheme:
    // "<component>::<subcomponent_id>". For example, "HaTS::WebDialog"
    explicit OTRProfileID(const std::string& profile_id);

    OTRProfileID() = default;

    // Returns this OTRProfileID in a string format that can be used for debug
    // message.
    const std::string& ToString() const;

    const std::string profile_id_;
  };

  class Delegate {
   public:
    virtual ~Delegate();

    // Called when creation of the profile is started.
    virtual void OnProfileCreationStarted(Profile* profile,
                                          CreateMode create_mode) = 0;

    // Called when creation of the profile is finished.
    virtual void OnProfileCreationFinished(Profile* profile,
                                           CreateMode create_mode,
                                           bool success,
                                           bool is_new_profile) = 0;
  };

  // Key used to bind profile to the widget with which it is associated.
  static const char kProfileKey[];

  explicit Profile(const OTRProfileID* otr_profile_id);
  Profile(const Profile&) = delete;
  Profile& operator=(const Profile&) = delete;
  ~Profile() override;

  // Profile prefs are registered as soon as the prefs are loaded for the first
  // time.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Create a new profile given a path. If `create_mode` is kAsynchronous then
  // the profile is initialized asynchronously.
  // Can return null if `create_mode` is kSynchronous and the creation of
  // the profile directory fails.
  static std::unique_ptr<Profile> CreateProfile(const base::FilePath& path,
                                                Delegate* delegate,
                                                CreateMode create_mode);

  // Returns the profile corresponding to the given browser context.
  static Profile* FromBrowserContext(content::BrowserContext* browser_context);

  // Returns the profile corresponding to the given WebUI.
  static Profile* FromWebUI(content::WebUI* web_ui);

  void AddObserver(ProfileObserver* observer);
  void RemoveObserver(ProfileObserver* observer);

  // content::BrowserContext implementation ------------------------------------

  // Returns the path of the directory where this context's data is stored.
  base::FilePath GetPath() override = 0;
  virtual base::FilePath GetPath() const = 0;

  // Returns the base name of the profile, which is the profile directory name
  // within the user data directory, e.g. "Default", "Profile 1", "Profile 2".
  base::FilePath GetBaseName() const;

  // Similar to GetBaseName(), but returns a string for debugging.
  std::string GetDebugName() const;

  // Return whether this context is off the record.
  // Note that for Chrome this covers BOTH Incognito mode and Guest sessions.
  bool IsOffTheRecord() final;
  bool IsOffTheRecord() const { return otr_profile_id_.has_value(); }
  const OTRProfileID& GetOTRProfileID() const;

  variations::VariationsClient* GetVariationsClient() override;

  // Returns the creation time of this profile. This will either be the creation
  // time of the profile directory or, for ephemeral off-the-record profiles,
  // the creation time of the profile object instance.
  virtual base::Time GetCreationTime() const = 0;

  // Typesafe downcast.
  virtual TestingProfile* AsTestingProfile();

  // Returns sequenced task runner where browser context dependent I/O
  // operations should be performed.
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() = 0;

  // Returns the username associated with this profile, if any. In non-test
  // implementations, this is usually the Google-services email address.
  virtual std::string GetProfileUserName() const = 0;

  // Return an OffTheRecord version of this profile with the given
  // |otr_profile_id|. The returned pointer is owned by the receiving profile.
  // If an OffTheRecord with |otr_profile_id| profile id does not exist, a new
  // profile is created and returned if |create_if_needed| is true or a nullptr
  // is returned if it is false.
  // If the receiving profile is OffTheRecord, the owner would be its original
  // profile.
  //
  // WARNING: Once a profile is no longer used, use
  // ProfileDestroyer::DestroyProfileWhenAppropriate or
  // ProfileDestroyer::DestroyOffTheRecordProfileNow to destroy it.
  virtual Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                          bool create_if_needed) = 0;

  // Returns all OffTheRecord profiles.
  virtual std::vector<Profile*> GetAllOffTheRecordProfiles() = 0;

  // Returns the primary OffTheRecord profile. Creates the profile if it doesn't
  // exist. If primary OffTheRecord profile does not exist and
  // |create_if_needed| is true, a new profile is created, otherwise nullptr is
  // returned.
  Profile* GetPrimaryOTRProfile(bool create_if_needed);

  // Destroys the OffTheRecord profile.
  virtual void DestroyOffTheRecordProfile(Profile* otr_profile) = 0;

  // True if an OffTheRecord profile with given id exists.
  virtual bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) = 0;

  // Returns true if the profile has any OffTheRecord profiles.
  virtual bool HasAnyOffTheRecordProfile() = 0;

  // True if the primary OffTheRecord profile exists.
  bool HasPrimaryOTRProfile();

  // Return the original "recording" profile. This method returns this if the
  // profile is not OffTheRecord.
  virtual Profile* GetOriginalProfile() = 0;

  // Return the original "recording" profile. This method returns this if the
  // profile is not OffTheRecord.
  virtual const Profile* GetOriginalProfile() const = 0;

  // Returns whether the profile is associated with the account of a child.
  // This method should not be used in new code to gate child-specific
  // functionality. Prefer a feture specific method
  // (eg. `SupervisedUserService::IsURLFilteringEnabled()`) or alternatively
  // use `SupervisedUserService::IsSubjectToParentalControls()`.
  virtual bool IsChild() const = 0;

  // Returns whether opening browser windows is allowed in this profile. For
  // example, browser windows are not allowed in Sign-in profile on Chrome OS.
  virtual bool AllowsBrowserWindows() const = 0;

  // Accessor. The instance is created upon first access.
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() = 0;

  // Retrieves a pointer to the PrefService that manages the
  // preferences for this user profile.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

  // Retrieves a pointer to the PrefService that manages the default zoom
  // level and the per-host zoom levels for this user profile.
  // TODO(wjmaclean): Remove this when HostZoomMap migrates to StoragePartition.
  virtual ChromeZoomLevelPrefs* GetZoomLevelPrefs();

  // Gives a read-only view of prefs that can be used even if there's no OTR
  // profile at the moment (i.e. HasOffTheRecordProfile is false).
  virtual PrefService* GetReadOnlyOffTheRecordPrefs();

  // Returns the main URLLoaderFactory.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Return whether two profiles are the same or one is the OffTheRecord version
  // of the other.
  virtual bool IsSameOrParent(Profile* profile) = 0;

  // Returns the time the profile was started. This is not the time the profile
  // was created, rather it is the time the user started chrome and logged into
  // this profile. For the single profile case, this corresponds to the time
  // the user started chrome.
  virtual base::Time GetStartTime() const = 0;

  // Returns the key used to index KeyedService instances created by a
  // SimpleKeyedServiceFactory, more strictly typed as a ProfileKey.
  virtual ProfileKey* GetProfileKey() const = 0;

  // Returns the SchemaRegistryService.
  virtual policy::SchemaRegistryService* GetPolicySchemaRegistryService() = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns the UserCloudPolicyManagerAsh.
  virtual policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManagerAsh() = 0;
#else
  // Returns the UserCloudPolicyManager.
  virtual policy::UserCloudPolicyManager* GetUserCloudPolicyManager() = 0;
  virtual policy::ProfileCloudPolicyManager* GetProfileCloudPolicyManager() = 0;
#endif

  // Returns CloudPolicyManager.
  // This function combine three Get*CloudPolicyManager functions above and
  // always returns the one that is currently activated.
  //
  // Returns UserCloudPolicyManagerAsh on Ash.
  // For others, returns UserCloudPolicyManager if it exists, otherwise use
  // ProfileCloudPolicyManager.
  virtual policy::CloudPolicyManager* GetCloudPolicyManager() = 0;

  virtual policy::ProfilePolicyConnector* GetProfilePolicyConnector() = 0;
  virtual const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const = 0;

  // Returns the last directory that was chosen for uploading or opening a file.
  virtual base::FilePath last_selected_directory() = 0;
  virtual void set_last_selected_directory(const base::FilePath& path) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  enum AppLocaleChangedVia{// Caused by chrome://settings change.
                           APP_LOCALE_CHANGED_VIA_SETTINGS,
                           // Locale has been reverted via LocaleChangeGuard.
                           APP_LOCALE_CHANGED_VIA_REVERT,
                           // From login screen.
                           APP_LOCALE_CHANGED_VIA_LOGIN,
                           // From login to a public session.
                           APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN,
                           // From AllowedLanguages policy.
                           APP_LOCALE_CHANGED_VIA_POLICY,
                           // Locale is reverted in the next demo session.
                           APP_LOCALE_CHANGED_VIA_DEMO_SESSION_REVERT,
                           // From system tray.
                           APP_LOCALE_CHANGED_VIA_SYSTEM_TRAY,
                           // Source unknown.
                           APP_LOCALE_CHANGED_VIA_UNKNOWN};

  // Changes application locale for a profile.
  virtual void ChangeAppLocale(
      const std::string& locale, AppLocaleChangedVia via) = 0;

  // Called after login.
  virtual void OnLogin() = 0;

  // Initializes Chrome OS's preferences.
  virtual void InitChromeOSPreferences() = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Returns the home page for this profile.
  virtual GURL GetHomePage() = 0;

  // Returns whether or not the profile was created by a version of Chrome
  // more recent (or equal to) the one specified.
  virtual bool WasCreatedByVersionOrLater(const std::string& version) = 0;

  // IsRegularProfile(), IsSystemProfile(), IsIncognitoProfile(), and
  // IsGuestSession() are mutually exclusive.
  // Note: IsGuestSession() is not mutually exclusive with the rest of the
  // methods mentioned above on Ash. TODO(crbug.com/40233408).
  //
  // IsSystemProfile() returns true for both regular and off-the-record profile
  //   of the system profile.
  // IsOffTheRecord() is true for the off the record profile of Incognito mode,
  // system profile, Guest sessions, and also non-primary OffTheRecord profiles.

  // Returns whether it's a regular profile.
  bool IsRegularProfile() const;

  // Returns whether it is an Incognito profile. An Incognito profile is an
  // off-the-record profile that is used for incognito mode.
  bool IsIncognitoProfile() const;

  // Returns true if this is a primary OffTheRecord profile, which covers the
  // OffTheRecord profile used for incognito mode and guest sessions.
  bool IsPrimaryOTRProfile() const;

  // Returns whether it is a Guest session. This covers both regular and
  // off-the-record profiles of a Guest session.
  virtual bool IsGuestSession() const;

  // Returns whether it is a system profile.
  bool IsSystemProfile() const;

  bool CanUseDiskWhenOffTheRecord() override;

  // Did the user restore the last session? This is set by SessionRestore.
  void set_restored_last_session(bool restored_last_session) {
    restored_last_session_ = restored_last_session;
  }
  bool restored_last_session() const {
    return restored_last_session_;
  }

  // Returns whether session cookies are restored and saved. The value is
  // ignored for in-memory profiles.
  virtual bool ShouldRestoreOldSessionCookies();
  virtual bool ShouldPersistSessionCookies() const;

  // Stop sending accessibility events until ResumeAccessibilityEvents().
  // Calls to Pause nest; no events will be sent until the number of
  // Resume calls matches the number of Pause calls received.
  void PauseAccessibilityEvents() {
    accessibility_pause_level_++;
  }

  void ResumeAccessibilityEvents() {
    DCHECK_GT(accessibility_pause_level_, 0);
    accessibility_pause_level_--;
  }

  bool ShouldSendAccessibilityEvents() {
    return 0 == accessibility_pause_level_;
  }

  // Returns whether the profile is new.  A profile is new if the browser has
  // not been shut down since the profile was created.
  virtual bool IsNewProfile() const = 0;

  // Notify observers of |OnProfileWillBeDestroyed| for this profile, if it has
  // not already been called. It is necessary because most Profiles are
  // destroyed by ProfileDestroyer, but in tests, some are not.
  void MaybeSendDestroyedNotification();

  // Convenience method to retrieve the default zoom level for the default
  // storage partition.
  double GetDefaultZoomLevelForProfile();

  // Wipes all data for this profile.
  void Wipe();

  virtual void SetCreationTimeForTesting(base::Time creation_time) = 0;

  virtual void RecordPrimaryMainFrameNavigation() = 0;

  base::WeakPtr<const Profile> GetWeakPtr() const;
  base::WeakPtr<Profile> GetWeakPtr();

  // Experimental getters/setters to gauge the performance of caching
  // frequently used KeyedServices in a Profile pointer.
  void set_theme_service(ThemeService* theme_service) {
    theme_service_ = theme_service;
  }
  const std::optional<raw_ptr<ThemeService>>& theme_service() {
    return theme_service_;
  }
  void set_instant_service(InstantService* instant_service) {
    instant_service_ = instant_service;
  }
  const std::optional<raw_ptr<InstantService>>& instant_service() {
    return instant_service_;
  }

#if BUILDFLAG(IS_ANDROID)
  static Profile* FromJavaObject(const jni_zero::JavaRef<jobject>& obj);
  jni_zero::ScopedJavaLocalRef<jobject> GetJavaObject() const;
#endif  // BUILDFLAG(IS_ANDROID)
 protected:
  // Creates an OffTheRecordProfile which points to this Profile.
  static std::unique_ptr<Profile> CreateOffTheRecordProfile(
      Profile* parent,
      const OTRProfileID& otr_profile_id);

  // Returns a newly created ExtensionPrefStore suitable for the supplied
  // Profile.
  static PrefStore* CreateExtensionPrefStore(Profile*,
                                             bool incognito_pref_store);

  void NotifyOffTheRecordProfileCreated(Profile* off_the_record);
  void NotifyProfileInitializationComplete();

  // Returns whether the user has signed in this profile to an account.
  virtual bool IsSignedIn() = 0;

 protected:
  const std::optional<OTRProfileID> otr_profile_id_;

 private:
  bool restored_last_session_ = false;

  // Used to prevent the notification that this Profile is destroyed from
  // being sent twice.
  bool sent_destroyed_notification_ = false;

  // Accessibility events will only be propagated when the pause
  // level is zero.  PauseAccessibilityEvents and ResumeAccessibilityEvents
  // increment and decrement the level, respectively, rather than set it to
  // true or false, so that calls can be nested.
  int accessibility_pause_level_ = 0;

  // Experimental objects to gauge the performance of caching frequently used
  // KeyedServices in a Profile pointer.
  std::optional<raw_ptr<ThemeService>> theme_service_;
  std::optional<raw_ptr<InstantService>> instant_service_;

  base::ObserverList<ProfileObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;

  class ChromeVariationsClient;

  // This member is lazily created. Once it is is created its lifetime must
  // match that of Profile itself.
  std::unique_ptr<variations::VariationsClient> chrome_variations_client_;

#if BUILDFLAG(IS_ANDROID)
  void InitJavaObject();
  void NotifyJavaOnProfileWillBeDestroyed();
  void DestroyJavaObject();

  jni_zero::ScopedJavaGlobalRef<jobject> j_obj_;
#endif
  base::WeakPtrFactory<Profile> weak_factory_{this};
};

// The comparator for profile pointers as key in a map.
struct ProfileCompare {
  bool operator()(Profile* a, Profile* b) const;
};

std::ostream& operator<<(std::ostream& out,
                         const Profile::OTRProfileID& profile_id);

#if BUILDFLAG(IS_ANDROID)
namespace jni_zero {
template <>
inline Profile* FromJniType<Profile*>(JNIEnv* env,
                                      const JavaRef<jobject>& j_profile) {
  return Profile::FromJavaObject(j_profile);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<Profile>(JNIEnv* env,
                                                      const Profile& profile) {
  return profile.GetJavaObject();
}
}  // namespace jni_zero
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // CHROME_BROWSER_PROFILES_PROFILE_H_
