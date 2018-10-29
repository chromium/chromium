// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/os_crypt/os_crypt_switches.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_store_mac.h"
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
// Don't do anything. We're going to use the default store.
#elif defined(USE_X11)
#include "components/os_crypt/key_storage_util_linux.h"
#if defined(USE_GNOME_KEYRING)
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#endif
#if defined(USE_LIBSECRET)
#include "chrome/browser/password_manager/native_backend_libsecret.h"
#endif
#include "chrome/browser/password_manager/native_backend_kwallet_x.h"
#include "chrome/browser/password_manager/password_store_x.h"
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"
#endif

using password_manager::PasswordStore;

namespace {

#if defined(USE_X11)
constexpr LocalProfileId kInvalidLocalProfileId =
    static_cast<LocalProfileId>(0);
constexpr PasswordStoreX::MigrationToLoginDBStep
    kMigrationToLoginDBNotAttempted = PasswordStoreX::NOT_ATTEMPTED;
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
std::string GetSyncUsername(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return identity_manager ? identity_manager->GetPrimaryAccountInfo().email
                          : std::string();
}
#endif

}  // namespace

// static
scoped_refptr<PasswordStore> PasswordStoreFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
PasswordStoreFactory* PasswordStoreFactory::GetInstance() {
  return base::Singleton<PasswordStoreFactory>::get();
}

// static
void PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(
    Profile* profile) {
  scoped_refptr<PasswordStore> password_store =
      GetForProfile(profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!password_store)
    return;
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service,
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess(),
      content::GetNetworkConnectionTracker(), profile->GetPath());
}

PasswordStoreFactory::PasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // TODO(crbug.com/715987). Remove when PasswordReuseDetector is decoupled
  // from PasswordStore.
  DependsOn(IdentityManagerFactory::GetInstance());
#endif
}

PasswordStoreFactory::~PasswordStoreFactory() {}

#if defined(USE_X11)
LocalProfileId PasswordStoreFactory::GetLocalProfileId(
    PrefService* prefs) const {
  LocalProfileId id =
      prefs->GetInteger(password_manager::prefs::kLocalProfileId);
  if (id == kInvalidLocalProfileId) {
    // Note that there are many more users than this. Thus, by design, this is
    // not a unique id. However, it is large enough that it is very unlikely
    // that it would be repeated twice on a single machine. It is still possible
    // for that to occur though, so the potential results of it actually
    // happening should be considered when using this value.
    static const int kLocalProfileIdMask = (1 << 24) - 1;
    do {
      id = base::RandInt(0, kLocalProfileIdMask);
      // TODO(mdm): scan other profiles to make sure they are not using this id?
    } while (id == kInvalidLocalProfileId);
    prefs->SetInteger(password_manager::prefs::kLocalProfileId, id);
  }
  return id;
}
#endif

scoped_refptr<RefcountedKeyedService>
PasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_WIN)
  password_manager_util_win::DelayReportOsPassword();
#endif
  Profile* profile = static_cast<Profile*>(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(profile->GetPath()));
#if defined(OS_MACOSX)
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  login_db->InitPasswordRecoveryUtil(
      std::make_unique<password_manager::PasswordRecoveryUtilMac>(
          local_state, base::ThreadTaskRunnerHandle::Get()));
#endif

  scoped_refptr<PasswordStore> ps;
#if defined(OS_WIN)
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#elif defined(OS_MACOSX)
  ps = new PasswordStoreMac(std::move(login_db), profile->GetPrefs());
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // For now, we use PasswordStoreDefault. We might want to make a native
  // backend for PasswordStoreX (see below) in the future though.
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#elif defined(USE_X11)
  // On POSIX systems, we try to use the "native" password management system of
  // the desktop environment currently running, allowing GNOME Keyring in XFCE.
  // (In all cases we fall back on the basic store in case of failure.)
  base::nix::DesktopEnvironment desktop_env = GetDesktopEnvironment();
  std::string store_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPasswordStore);
  LinuxBackendUsed used_backend = PLAINTEXT;

  PrefService* prefs = profile->GetPrefs();
  LocalProfileId id = GetLocalProfileId(prefs);

  bool use_preference = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableEncryptionSelection);
  bool use_backend = true;
  if (use_preference) {
    base::FilePath user_data_dir;
    chrome::GetDefaultUserDataDirectory(&user_data_dir);
    use_backend = os_crypt::GetBackendUse(user_data_dir);
  }

  os_crypt::SelectedLinuxBackend selected_backend =
      os_crypt::SelectBackend(store_type, use_backend, desktop_env);

  std::unique_ptr<PasswordStoreX::NativeBackend> backend;
  if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5) {
    VLOG(1) << "Trying KWallet for password storage.";
    base::nix::DesktopEnvironment used_desktop_env =
        selected_backend == os_crypt::SelectedLinuxBackend::KWALLET
            ? base::nix::DESKTOP_ENVIRONMENT_KDE4
            : base::nix::DESKTOP_ENVIRONMENT_KDE5;
    backend.reset(new NativeBackendKWallet(id, used_desktop_env));
    if (backend->Init()) {
      VLOG(1) << "Using KWallet for password storage.";
      used_backend = KWALLET;
    } else {
      backend.reset();
    }
  } else if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
             selected_backend ==
                 os_crypt::SelectedLinuxBackend::GNOME_KEYRING ||
             selected_backend ==
                 os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET) {
#if defined(USE_LIBSECRET)
    if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
        selected_backend == os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET) {
      VLOG(1) << "Trying libsecret for password storage.";
      backend.reset(new NativeBackendLibsecret(id));
      if (backend->Init()) {
        VLOG(1) << "Using libsecret keyring for password storage.";
        used_backend = LIBSECRET;
      } else {
        backend.reset();
      }
    }
#endif  // defined(USE_LIBSECRET)
#if defined(USE_GNOME_KEYRING)
    if (!backend.get() &&
        (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
         selected_backend == os_crypt::SelectedLinuxBackend::GNOME_KEYRING)) {
      VLOG(1) << "Trying GNOME keyring for password storage.";
      backend.reset(new NativeBackendGnome(id));
      if (backend->Init()) {
        VLOG(1) << "Using GNOME keyring for password storage.";
        used_backend = GNOME_KEYRING;
      } else {
        backend.reset();
      }
    }
#endif  // defined(USE_GNOME_KEYRING)
  }

  if (!backend.get()) {
    LOG(WARNING) << "Using basic (unencrypted) store for password storage. "
        "See "
        "https://chromium.googlesource.com/chromium/src/+/master/docs/linux_password_storage.md"
        " for more information about password storage options.";
  }

  ps = new PasswordStoreX(
      std::move(login_db),
      profile->GetPath().Append(password_manager::kLoginDataFileName),
      profile->GetPath().Append(password_manager::kSecondLoginDataFileName),
      std::move(backend), prefs);
  RecordBackendStatistics(desktop_env, store_type, used_backend);
#elif defined(USE_OZONE)
  ps = new password_manager::PasswordStoreDefault(std::move(login_db));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);
  if (!ps->Init(sync_start_util::GetFlareForSyncableService(profile->GetPath()),
                profile->GetPrefs())) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Prepare password hash data for reuse detection.
  ps->PreparePasswordHashData(GetSyncUsername(profile));
#endif

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile))
          return nullptr;
        return content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetNetworkContext();
      },
      profile);
  password_manager_util::RemoveUselessCredentials(ps, profile->GetPrefs(), 60,
                                                  network_context_getter);

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  std::unique_ptr<password_manager::PasswordStoreSigninNotifier> notifier =
      std::make_unique<password_manager::PasswordStoreSigninNotifierImpl>(
          profile);
  ps->SetPasswordStoreSigninNotifier(std::move(notifier));
#endif

  return ps;
}

void PasswordStoreFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(USE_X11)
  // Notice that the preprocessor conditions above are exactly those that will
  // result in using PasswordStoreX in BuildServiceInstanceFor().
  registry->RegisterIntegerPref(password_manager::prefs::kLocalProfileId,
                                kInvalidLocalProfileId);
  registry->RegisterIntegerPref(
      password_manager::prefs::kMigrationToLoginDBStep,
      kMigrationToLoginDBNotAttempted);
#endif
}

content::BrowserContext* PasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

#if defined(USE_X11)
base::nix::DesktopEnvironment PasswordStoreFactory::GetDesktopEnvironment() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetDesktopEnvironment(env.get());
}

void PasswordStoreFactory::RecordBackendStatistics(
    base::nix::DesktopEnvironment desktop_env,
    const std::string& command_line_flag,
    LinuxBackendUsed used_backend) {
  LinuxBackendUsage usage = OTHER_PLAINTEXT;
  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    if (command_line_flag == "kwallet") {
      usage = used_backend == KWALLET ? KDE_KWALLETFLAG_KWALLET
                                      : KDE_KWALLETFLAG_PLAINTEXT;
    } else if (command_line_flag == "gnome") {
      usage = used_backend == PLAINTEXT
                  ? KDE_GNOMEFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? KDE_GNOMEFLAG_KEYRING
                                                   : KDE_GNOMEFLAG_LIBSECRET);
    } else if (command_line_flag == "basic") {
      usage = KDE_BASICFLAG_PLAINTEXT;
    } else {
      usage =
          used_backend == KWALLET ? KDE_NOFLAG_KWALLET : KDE_NOFLAG_PLAINTEXT;
    }
  } else if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_CINNAMON ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
    if (command_line_flag == "kwallet") {
      usage = used_backend == KWALLET ? GNOME_KWALLETFLAG_KWALLET
                                      : GNOME_KWALLETFLAG_PLAINTEXT;
    } else if (command_line_flag == "gnome") {
      usage = used_backend == PLAINTEXT
                  ? GNOME_GNOMEFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? GNOME_GNOMEFLAG_KEYRING
                                                   : GNOME_GNOMEFLAG_LIBSECRET);
    } else if (command_line_flag == "basic") {
      usage = GNOME_BASICFLAG_PLAINTEXT;
    } else {
      usage = used_backend == PLAINTEXT
                  ? GNOME_NOFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? GNOME_NOFLAG_KEYRING
                                                   : GNOME_NOFLAG_LIBSECRET);
    }
  } else {
    // It is neither Gnome nor KDE environment.
    switch (used_backend) {
      case PLAINTEXT:
        usage = OTHER_PLAINTEXT;
        break;
      case KWALLET:
        usage = OTHER_KWALLET;
        break;
      case GNOME_KEYRING:
        usage = OTHER_KEYRING;
        break;
      case LIBSECRET:
        usage = OTHER_LIBSECRET;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LinuxBackendStatistics", usage,
                            MAX_BACKEND_USAGE_VALUE);
}
#endif
