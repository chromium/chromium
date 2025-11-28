// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <sstream>
#include <string>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/android/jni_headers/OtrProfileId_jni.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_pref_store.h"              // nogncheck
#include "extensions/browser/extension_pref_value_map_factory.h"  // nogncheck
#include "extensions/browser/pref_names.h"                        // nogncheck
#endif

#if DCHECK_IS_ON()

#include <set>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace {

base::Lock& GetProfileInstancesLock() {
  static base::NoDestructor<base::Lock> profile_instances_lock;
  return *profile_instances_lock;
}

std::set<content::BrowserContext*>& GetProfileInstances() {
  static base::NoDestructor<std::set<content::BrowserContext*>>
      profile_instances;
  return *profile_instances;
}

}  // namespace

#endif  // DCHECK_IS_ON()

namespace {

const char kDevToolsOTRProfileIDPrefix[] = "Devtools::BrowserContext";
const char kMediaRouterOTRProfileIDPrefix[] = "MediaRouter::Presentation";
const char kTestOTRProfileIDPrefix[] = "Test::OTR";

#if BUILDFLAG(IS_CHROMEOS)
const char kCaptivePortalOTRProfileIDPrefix[] = "CaptivePortal::Signin";
#endif

using perfetto::protos::pbzero::ChromeTrackEvent;

}  // namespace

Profile::OTRProfileID::OTRProfileID(const std::string& profile_id)
    : profile_id_(profile_id) {}

bool Profile::OTRProfileID::AllowsBrowserWindows() const {
  // Non-Primary OTR profiles are not supposed to create Browser windows.
  // DevTools::BrowserContext, MediaRouter::Presentation, and
  // CaptivePortal::Signin are exceptions to this ban.
  if (*this == PrimaryID() || IsDevTools() ||
      base::StartsWith(profile_id_, kMediaRouterOTRProfileIDPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (base::StartsWith(profile_id_, kCaptivePortalOTRProfileIDPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
#endif
  return false;
}

bool Profile::OTRProfileID::IsDevTools() const {
  return base::StartsWith(profile_id_, kDevToolsOTRProfileIDPrefix,
                          base::CompareCase::SENSITIVE);
}

#if BUILDFLAG(IS_CHROMEOS)
bool Profile::OTRProfileID::IsCaptivePortal() const {
  return base::StartsWith(profile_id_, kCaptivePortalOTRProfileIDPrefix,
                          base::CompareCase::SENSITIVE);
}
#endif

// static
const Profile::OTRProfileID Profile::OTRProfileID::PrimaryID() {
  // OTRProfileID value should be same as
  // |OtrProfileId.java#sPrimaryOtrProfileId| variable.
  return OTRProfileID("profile::primary_otr");
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUnique(
    const std::string& profile_id_prefix) {
  return OTRProfileID(base::StringPrintf(
      "%s-%s", profile_id_prefix.c_str(),
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForDevTools() {
  return CreateUnique(kDevToolsOTRProfileIDPrefix);
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForMediaRouter() {
  return CreateUnique(kMediaRouterOTRProfileIDPrefix);
}

#if BUILDFLAG(IS_CHROMEOS)
// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForCaptivePortal() {
  return CreateUnique(kCaptivePortalOTRProfileIDPrefix);
}
#endif

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForTesting() {
  return CreateUnique(kTestOTRProfileIDPrefix);
}

const std::string& Profile::OTRProfileID::ToString() const {
  return profile_id_;
}

std::ostream& operator<<(std::ostream& out,
                         const Profile::OTRProfileID& profile_id) {
  out << profile_id.ToString();
  return out;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
Profile::OTRProfileID::ConvertToJavaOTRProfileID(JNIEnv* env) const {
  return Java_OtrProfileId_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, profile_id_));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::ConvertFromJavaOTRProfileID(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_otr_profile_id) {
  return OTRProfileID(base::android::ConvertJavaStringToUTF8(
      env, Java_OtrProfileId_getProfileId(env, j_otr_profile_id)));
}

// static
static base::android::ScopedJavaLocalRef<jobject>
JNI_OtrProfileId_CreateUniqueOtrProfileId(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_profile_id_prefix) {
  Profile::OTRProfileID profile_id = Profile::OTRProfileID::CreateUnique(
      base::android::ConvertJavaStringToUTF8(env, j_profile_id_prefix));
  return profile_id.ConvertToJavaOTRProfileID(env);
}

// static
static base::android::ScopedJavaLocalRef<jobject> JNI_OtrProfileId_GetPrimaryId(
    JNIEnv* env) {
  return Profile::OTRProfileID::PrimaryID().ConvertToJavaOTRProfileID(env);
}

// static
Profile::OTRProfileID Profile::OTRProfileID::Deserialize(
    const std::string& value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_value =
      base::android::ConvertUTF8ToJavaString(env, value);
  base::android::ScopedJavaLocalRef<jobject> j_otr_profile_id =
      Java_OtrProfileId_deserializeWithoutVerify(env, j_value);
  return ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
}

std::string Profile::OTRProfileID::Serialize() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_OtrProfileId_serialize(env, ConvertToJavaOTRProfileID(env)));
}
#endif  // BUILDFLAG(IS_ANDROID)

Profile::Profile(const OTRProfileID* otr_profile_id)
    : otr_profile_id_(otr_profile_id ? std::make_optional(*otr_profile_id)
                                     : std::nullopt) {
#if BUILDFLAG(IS_CHROMEOS)
  new_guest_profile_impl_ =
      base::FeatureList::IsEnabled(chromeos::features::kNewGuestProfile);
#endif

#if DCHECK_IS_ON()
  base::AutoLock lock(GetProfileInstancesLock());
  GetProfileInstances().insert(this);
#endif  // DCHECK_IS_ON()

  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(this);

#if BUILDFLAG(IS_ANDROID)
  InitJavaObject();
#endif
}

Profile::~Profile() {
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif

#if DCHECK_IS_ON()
  base::AutoLock lock(GetProfileInstancesLock());
  GetProfileInstances().erase(this);
#endif  // DCHECK_IS_ON()
}

// static
Profile* Profile::FromBrowserContext(content::BrowserContext* browser_context) {
  if (!browser_context)
    return nullptr;

  // For code running in a chrome/ environment, it is safe to cast to Profile*
  // because Profile is the only implementation of BrowserContext used. In
  // testing, however, there are several BrowserContext subclasses that are not
  // Profile subclasses, and we can catch them. http://crbug.com/725276
#if DCHECK_IS_ON()
  base::AutoLock lock(GetProfileInstancesLock());
  if (!GetProfileInstances().count(browser_context)) {
    DCHECK(false)
        << "Non-Profile BrowserContext passed to Profile::FromBrowserContext! "
           "If you have a test linked in chrome/ you need a chrome/ based test "
           "class such as TestingProfile in chrome/test/base/testing_profile.h "
           "or you need to subclass your test class from Profile, not from "
           "BrowserContext.";
  }
#endif  // DCHECK_IS_ON()
  return static_cast<Profile*>(browser_context);
}

// static
Profile* Profile::FromWebUI(content::WebUI* web_ui) {
  return FromBrowserContext(web_ui->GetWebContents()->GetBrowserContext());
}

void Profile::AddObserver(ProfileObserver* observer) {
  observers_.AddObserver(observer);
}

void Profile::RemoveObserver(ProfileObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::FilePath Profile::GetBaseName() const {
  return GetPath().BaseName();
}

std::string Profile::GetDebugName() const {
  std::string name = GetBaseName().MaybeAsASCII();
  return name.empty() ? "UnknownProfile" : name;
}

TestingProfile* Profile::AsTestingProfile() {
  return nullptr;
}

ChromeZoomLevelPrefs* Profile::GetZoomLevelPrefs() {
  return nullptr;
}

Profile::Delegate::~Delegate() = default;

// static
const char Profile::kProfileKey[] = "__PROFILE__";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kContextualSearchWasFullyPrivacyEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kContextualSearchPromoCardShownCount, 0);
#endif  // BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(prefs::kSessionExitType, std::string());
  registry->RegisterBooleanPref(prefs::kDisableExtensions, false);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  registry->RegisterBooleanPref(extensions::pref_names::kAlertsInitialized,
                                false);
#endif
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  registry->RegisterStringPref(prefs::kSelectFileLastDirectory,
                               home.MaybeAsASCII());
#if BUILDFLAG(IS_CHROMEOS)
  const uint32_t caption_registration_flags =
      base::FeatureList::IsEnabled(
          ash::features::kOsSyncAccessibilitySettingsBatch2)
          ? user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF
          : 0;
#else
  constexpr uint32_t caption_registration_flags = 0;
#endif
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextSize,
                               std::string(), caption_registration_flags);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextFont,
                               std::string(), caption_registration_flags);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextColor,
                               std::string(), caption_registration_flags);
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsTextOpacity, 100,
                                caption_registration_flags);
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsBackgroundOpacity,
                                100, caption_registration_flags);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsBackgroundColor,
                               std::string(), caption_registration_flags);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextShadow,
                               std::string(), caption_registration_flags);
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
  registry->RegisterStringPref(prefs::kPreinstalledApps, "install");
  registry->RegisterIntegerPref(prefs::kProfileIconVersion, 0);
  registry->RegisterBooleanPref(prefs::kProfileIconWin11Format, false);
  registry->RegisterBooleanPref(prefs::kAllowDinosaurEasterEgg, true);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(chromeos::prefs::kCaptivePortalSignin, false);
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  registry->RegisterStringPref(
      language::prefs::kApplicationLocale, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterStringPref(prefs::kApplicationLocaleBackup, std::string());
  registry->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                               std::string());
#endif

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(prefs::kLatestVersionWhenClickedUpdateMenuItem,
                               std::string());
  registry->RegisterStringPref(prefs::kCommerceMerchantViewerMessagesShownTime,
                               std::string());
#endif

  registry->RegisterDictionaryPref(prefs::kWebShareVisitedTargets);
  registry->RegisterDictionaryPref(
      prefs::kProtocolHandlerPerOriginAllowedProtocols);

  registry->RegisterListPref(prefs::kAutoLaunchProtocolsFromOrigins);

  // Instead of registering new prefs here, please create a static method and
  // invoke it from RegisterProfilePrefs() in
  // chrome/browser/prefs/browser_prefs.cc.
}

bool Profile::IsRegularProfile() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kRegular;
}

bool Profile::IsIncognitoProfile() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kIncognito;
}

bool Profile::IsGuestSession() const {
#if BUILDFLAG(IS_CHROMEOS)
  if (!new_guest_profile_impl_) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        ash::switches::kGuestSession);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kGuest;
}

PrefService* Profile::GetReadOnlyOffTheRecordPrefs() {
  return nullptr;
}

bool Profile::IsSystemProfile() const {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  DCHECK_NE(profile_metrics::GetBrowserProfileType(this),
            profile_metrics::BrowserProfileType::kSystem);
  return false;
#else  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kSystem;
#endif
}

bool Profile::IsPrimaryOTRProfile() const {
  return otr_profile_id_.has_value() &&
         otr_profile_id_.value() == OTRProfileID::PrimaryID();
}

bool Profile::IsDevToolsOTRProfile() const {
  return otr_profile_id_.has_value() && otr_profile_id_->IsDevTools();
}

bool Profile::CanUseDiskWhenOffTheRecord() {
#if BUILDFLAG(IS_CHROMEOS)
  // Guest mode on ChromeOS uses an in-memory file system to store the profile
  // in, so despite this being an off the record profile, it is still okay to
  // store data on disk.
  return IsGuestSession();
#else
  return false;
#endif
}

bool Profile::ShouldRestoreOldSessionCookies() {
  return false;
}

bool Profile::ShouldPersistSessionCookies() const {
  return false;
}

void Profile::MaybeSendDestroyedNotification() {
  TRACE_EVENT("shutdown", "Profile::MaybeSendDestroyedNotification",
               ChromeTrackEvent::kChromeBrowserContext, this);

  if (sent_destroyed_notification_)
    return;
  sent_destroyed_notification_ = true;

  NotifyWillBeDestroyed();

#if BUILDFLAG(IS_ANDROID)
  NotifyJavaOnProfileWillBeDestroyed();
#endif

  for (auto& observer : observers_) {
    observer.OnProfileWillBeDestroyed(this);
  }
}

// static
PrefStore* Profile::CreateExtensionPrefStore(Profile* profile,
                                             bool incognito_pref_store) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ExtensionPrefValueMap* pref_value_map =
          ExtensionPrefValueMapFactory::GetForBrowserContext(profile)) {
    return new ExtensionPrefStore(pref_value_map, incognito_pref_store);
  }
#endif
  return nullptr;
}

bool ProfileCompare::operator()(Profile* a, Profile* b) const {
  DCHECK(a && b);
  if (a->IsSameOrParent(b))
    return false;
  return a->GetOriginalProfile() < b->GetOriginalProfile();
}

double Profile::GetDefaultZoomLevelForProfile() {
  return GetDefaultStoragePartition()->GetHostZoomMap()->GetDefaultZoomLevel();
}

void Profile::Wipe() {
  GetBrowsingDataRemover()->Remove(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::WIPE_PROFILE,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES);
}

void Profile::NotifyOffTheRecordProfileCreated(Profile* off_the_record) {
  DCHECK_EQ(off_the_record->GetOriginalProfile(), this);
  DCHECK(off_the_record->IsOffTheRecord());
  for (auto& observer : observers_)
    observer.OnOffTheRecordProfileCreated(off_the_record);
}

void Profile::NotifyProfileInitializationComplete() {
  DCHECK(!IsOffTheRecord());
  for (auto& observer : observers_) {
    observer.OnProfileInitializationComplete(this);
  }
}

Profile* Profile::GetPrimaryOTRProfile(bool create_if_needed) {
  return GetOffTheRecordProfile(OTRProfileID::PrimaryID(), create_if_needed);
}

const Profile::OTRProfileID& Profile::GetOTRProfileID() const {
  DCHECK(IsOffTheRecord());
  return otr_profile_id_.value();
}

bool Profile::HasPrimaryOTRProfile() {
  return HasOffTheRecordProfile(OTRProfileID::PrimaryID());
}

bool Profile::AllowsBrowserWindows() const {
  if (allows_browser_windows_for_testing_.has_value()) {
    CHECK_IS_TEST();
    return allows_browser_windows_for_testing_.value();
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Do not allow Browsers on signin-derived profiles.
  if (ash::IsSigninBrowserContext(GetOriginalProfile())) {
    return false;
  }
#endif
  // Only OTR Browsers may be opened in guest mode.
  if (IsGuestSession() && !IsOffTheRecord()) {
    return false;
  }

  // Some OTR profiles are not allowed to open Browsers.
  if (otr_profile_id_.has_value() && !otr_profile_id_->AllowsBrowserWindows()) {
    return false;
  }

  return !IsSystemProfile();
}

class Profile::ChromeVariationsClient : public variations::VariationsClient {
 public:
  explicit ChromeVariationsClient(Profile* profile) : profile_(profile) {}

  ~ChromeVariationsClient() override = default;

  bool IsOffTheRecord() const override { return profile_->IsOffTheRecord(); }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(profile_->IsSignedIn());
  }

 private:
  raw_ptr<Profile> profile_;
};

bool Profile::IsOffTheRecord() {
  return otr_profile_id_.has_value();
}

variations::VariationsClient* Profile::GetVariationsClient() {
  if (!chrome_variations_client_)
    chrome_variations_client_ = std::make_unique<ChromeVariationsClient>(this);
  return chrome_variations_client_.get();
}

base::WeakPtr<Profile> Profile::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::string Profile::ToDebugString() {
  std::ostringstream out;
  out << "(" << this << "):" << (IsRegularProfile() ? " regular" : "")
      << (IsIncognitoProfile() ? " incognito" : "")
      << (IsGuestSession() ? " guest" : "")
      << (IsSystemProfile() ? " system" : "");
  if (IsOffTheRecord()) {
    out << ", otr";
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (ash::IsSigninBrowserContext(this)) {
    out << ", signin";
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (GetOriginalProfile() == this) {
    out << ", is-original";
  } else {
    out << ", original=[" << GetOriginalProfile()->ToDebugString() << "]";
  }

  return out.str();
}

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(OtrProfileId)
#endif
