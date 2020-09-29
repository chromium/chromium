// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/constants/chromeos_switches.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/android/jni_headers/OTRProfileID_jni.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/first_run/first_run.h"
#include "content/public/browser/host_zoom_map.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/pref_names.h"
#endif

#if BUILDFLAG(IS_LACROS)
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

#if DCHECK_IS_ON()

#include <set>
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace {

base::LazyInstance<base::Lock>::Leaky g_profile_instances_lock =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::set<content::BrowserContext*>>::Leaky
    g_profile_instances = LAZY_INSTANCE_INITIALIZER;

}  // namespace

#endif  // DCHECK_IS_ON()

namespace {

class ChromeVariationsClient : public variations::VariationsClient {
 public:
  explicit ChromeVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~ChromeVariationsClient() override = default;

  bool IsOffTheRecord() const override {
    return browser_context_->IsOffTheRecord();
  }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(IsSignedIn());
  }

 private:
  bool IsSignedIn() const {
    Profile* profile = Profile::FromBrowserContext(browser_context_);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    return identity_manager && identity_manager->HasPrimaryAccount();
  }

  content::BrowserContext* browser_context_;
};

const char kDevToolsOTRProfileIDPrefix[] = "Devtools::BrowserContext";
}  // namespace

Profile::OTRProfileID::OTRProfileID(const std::string& profile_id)
    : profile_id_(profile_id) {}

bool Profile::OTRProfileID::AllowsBrowserWindows() const {
  // Non-Primary OTR profiles are not supposed to create Browser windows.
  // DevTools::BrowserContext is an exception to this ban.
  return *this == PrimaryID() ||
         base::StartsWith(profile_id_, kDevToolsOTRProfileIDPrefix,
                          base::CompareCase::SENSITIVE);
}

// static
const Profile::OTRProfileID Profile::OTRProfileID::PrimaryID() {
  return OTRProfileID("profile::primary_otr");
}

// static
int Profile::OTRProfileID::first_unused_index_ = 0;

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUnique(
    const std::string& profile_id_prefix) {
  return OTRProfileID(base::StringPrintf("%s-%i", profile_id_prefix.c_str(),
                                         first_unused_index_++));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForDevTools() {
  return CreateUnique(kDevToolsOTRProfileIDPrefix);
}

const std::string& Profile::OTRProfileID::ToString() const {
  return profile_id_;
}

std::ostream& operator<<(std::ostream& out,
                         const Profile::OTRProfileID& profile_id) {
  out << profile_id.ToString();
  return out;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
Profile::OTRProfileID::ConvertToJavaOTRProfileID(JNIEnv* env) const {
  return Java_OTRProfileID_Constructor(
      env, base::android::ConvertUTF16ToJavaString(
               env, base::ASCIIToUTF16(profile_id_)));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::ConvertFromJavaOTRProfileID(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_otr_profile_id) {
  return OTRProfileID(
      base::UTF16ToASCII(base::android::ConvertJavaStringToUTF16(
          env, Java_OTRProfileID_getProfileID(env, j_otr_profile_id))));
}

// static
base::android::ScopedJavaLocalRef<jobject>
JNI_OTRProfileID_CreateUniqueOTRProfileID(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_profile_id_prefix) {
  Profile::OTRProfileID profile_id =
      Profile::OTRProfileID::CreateUnique(base::UTF16ToASCII(
          base::android::ConvertJavaStringToUTF16(env, j_profile_id_prefix)));
  return profile_id.ConvertToJavaOTRProfileID(env);
}
#endif

Profile::Profile() {
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().insert(this);
#endif  // DCHECK_IS_ON()

  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(this);
}

Profile::~Profile() {
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().erase(this);
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
  base::AutoLock lock(g_profile_instances_lock.Get());
  if (!g_profile_instances.Get().count(browser_context)) {
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

TestingProfile* Profile::AsTestingProfile() {
  return nullptr;
}

#if !defined(OS_ANDROID)
ChromeZoomLevelPrefs* Profile::GetZoomLevelPrefs() {
  return nullptr;
}
#endif  // !defined(OS_ANDROID)

PrefService* Profile::GetReadOnlyOffTheRecordPrefs() {
  return GetOffTheRecordPrefs();
}

Profile::Delegate::~Delegate() {
}

// static
const char Profile::kProfileKey[] = "__PROFILE__";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  registry->RegisterStringPref(prefs::kSessionExitType, std::string());
  registry->RegisterInt64Pref(prefs::kSiteEngagementLastUpdateTime, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterBooleanPref(prefs::kSSLErrorOverrideAllowed, true);
  registry->RegisterBooleanPref(prefs::kDisableExtensions, false);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(extensions::pref_names::kAlertsInitialized,
                                false);
#endif
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  registry->RegisterStringPref(prefs::kSelectFileLastDirectory,
                               home.MaybeAsASCII());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextSize,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextFont,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextColor,
                               std::string());
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsTextOpacity, 100);
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsBackgroundOpacity,
                                100);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsBackgroundColor,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextShadow,
                               std::string());
#if !defined(OS_ANDROID)
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
#endif  // !defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kDefaultApps, "install");
  registry->RegisterBooleanPref(prefs::kSpeechRecognitionFilterProfanities,
                                true);
  registry->RegisterIntegerPref(prefs::kProfileIconVersion, 0);
  registry->RegisterBooleanPref(prefs::kAllowDinosaurEasterEgg, true);
#if defined(OS_CHROMEOS)
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

  data_reduction_proxy::RegisterSyncableProfilePrefs(registry);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Preferences related to the avatar bubble and user manager tutorials.
  registry->RegisterIntegerPref(prefs::kProfileAvatarTutorialShown, 0);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kClickedUpdateMenuItem, false);
  registry->RegisterStringPref(prefs::kLatestVersionWhenClickedUpdateMenuItem,
                               std::string());
#endif

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterCloudServicesPrefSet, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterEnableCloudServices, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  registry->RegisterListPref(
      media_router::prefs::kMediaRouterTabMirroringSources);
#endif

  registry->RegisterDictionaryPref(prefs::kWebShareVisitedTargets);
  registry->RegisterDictionaryPref(
      prefs::kProtocolHandlerPerOriginAllowedProtocols);

  registry->RegisterListPref(prefs::kAutoLaunchProtocolsFromOrigins);

  // Instead of registering new prefs here, please create a static method and
  // invoke it from RegisterProfilePrefs() in
  // chrome/browser/prefs/browser_prefs.cc.
}

std::string Profile::GetDebugName() const {
  std::string name = GetPath().BaseName().MaybeAsASCII();
  return name.empty() ? "UnknownProfile" : name;
}

bool Profile::IsRegularProfile() const {
  return !IsOffTheRecord();
}

bool Profile::IsIncognitoProfile() const {
  return IsPrimaryOTRProfile() && !IsGuestSession();
}

bool Profile::IsGuestSession() const {
#if defined(OS_CHROMEOS)
  static bool is_guest_session =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kGuestSession);
  return is_guest_session;
#else
#if BUILDFLAG(IS_LACROS)
  DCHECK(chromeos::LacrosChromeServiceImpl::Get());
  if (chromeos::LacrosChromeServiceImpl::Get()->init_params()->session_type !=
      crosapi::mojom::SessionType::kUnknown) {
    return chromeos::LacrosChromeServiceImpl::Get()
               ->init_params()
               ->session_type == crosapi::mojom::SessionType::kGuestSession;
  }
#endif  // BUILDFLAG(IS_LACROS)
  return is_guest_profile_;
#endif  // defined(OS_CHROMEOS)
}

bool Profile::IsSystemProfile() const {
  return is_system_profile_;
}

bool Profile::IsPrimaryOTRProfile() const {
  return IsOffTheRecord() && GetOTRProfileID() == OTRProfileID::PrimaryID();
}

bool Profile::CanUseDiskWhenOffTheRecord() {
#if defined(OS_CHROMEOS)
  // Guest mode on ChromeOS uses an in-memory file system to store the profile
  // in, so despite this being an off the record profile, it is still okay to
  // store data on disk.
  return IsGuestSession();
#else
  return false;
#endif
}

bool Profile::ShouldRestoreOldSessionCookies() const {
  return false;
}

bool Profile::ShouldPersistSessionCookies() const {
  return false;
}

void Profile::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  ProfileNetworkContextServiceFactory::GetForContext(this)
      ->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                      network_context_params,
                                      cert_verifier_creation_params);
}

bool Profile::IsNewProfile() const {
#if !defined(OS_ANDROID)
  // The profile is new if the preference files has just been created, except on
  // first run, because the installer may create a preference file. See
  // https://crbug.com/728402
  if (first_run::IsChromeFirstRun())
    return true;
#endif

  return GetOriginalProfile()->GetPrefs()->GetInitializationStatus() ==
         PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

void Profile::MaybeSendDestroyedNotification() {
  TRACE_EVENT1("shutdown", "Profile::MaybeSendDestroyedNotification", "profile",
               this);

  if (!sent_destroyed_notification_) {
    sent_destroyed_notification_ = true;

    NotifyWillBeDestroyed(this);

    for (auto& observer : observers_)
      observer.OnProfileWillBeDestroyed(this);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(this),
        content::NotificationService::NoDetails());
  }
}

// static
PrefStore* Profile::CreateExtensionPrefStore(Profile* profile,
                                             bool incognito_pref_store) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return new ExtensionPrefStore(
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      incognito_pref_store);
#else
  return nullptr;
#endif
}

bool ProfileCompare::operator()(Profile* a, Profile* b) const {
  DCHECK(a && b);
  if (a->IsSameOrParent(b))
    return false;
  return a->GetOriginalProfile() < b->GetOriginalProfile();
}

#if !defined(OS_ANDROID)
double Profile::GetDefaultZoomLevelForProfile() {
  return GetDefaultStoragePartition(this)
      ->GetHostZoomMap()
      ->GetDefaultZoomLevel();
}
#endif  // !defined(OS_ANDROID)

void Profile::Wipe() {
  content::BrowserContext::GetBrowsingDataRemover(this)->Remove(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::WIPE_PROFILE,
      ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES);
}

void Profile::NotifyOffTheRecordProfileCreated(Profile* off_the_record) {
  DCHECK_EQ(off_the_record->GetOriginalProfile(), this);
  DCHECK(off_the_record->IsOffTheRecord());
  for (auto& observer : observers_)
    observer.OnOffTheRecordProfileCreated(off_the_record);
}

Profile* Profile::GetPrimaryOTRProfile() {
  return GetOffTheRecordProfile(OTRProfileID::PrimaryID());
}

bool Profile::HasPrimaryOTRProfile() {
  return HasOffTheRecordProfile(OTRProfileID::PrimaryID());
}

variations::VariationsClient* Profile::GetVariationsClient() {
  if (!chrome_variations_client_)
    chrome_variations_client_ = std::make_unique<ChromeVariationsClient>(this);
  return chrome_variations_client_.get();
}
