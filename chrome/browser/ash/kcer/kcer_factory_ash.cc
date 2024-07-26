// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kcer/kcer_factory_ash.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/crosapi/chaps_service_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/kcer/nssdb_migration/kcer_rollback_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "chromeos/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/components/kcer/extra_instances.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"

namespace kcer {
namespace {

// Returns the currently valid ChapsService. Might return a nullptr during early
// initialization and after shutdown.
crosapi::mojom::ChapsService* GetChapsService() {
  crosapi::mojom::ChapsService* chaps_service = nullptr;
  if (crosapi::CrosapiManager::IsInitialized() &&
      crosapi::CrosapiManager::Get() &&
      crosapi::CrosapiManager::Get()->crosapi_ash()) {
    chaps_service =
        crosapi::CrosapiManager::Get()->crosapi_ash()->chaps_service_ash();
  }
  if (!chaps_service) {
    LOG(ERROR) << "ChapsService mojo interface is not available";
  }
  return chaps_service;
}

}  // namespace

const user_manager::User* GetUserByContext(content::BrowserContext* context) {
  if (!context) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }
  return ash::ProfileHelper::Get()->GetUserByProfile(profile);
}

KcerFactoryAsh::KcerFactoryAsh() = default;
KcerFactoryAsh::~KcerFactoryAsh() = default;

// static
KcerFactory* KcerFactoryAsh::GetInstance() {
  EnsureFactoryBuilt();
  return GetGlobalPointer();
}

// static
void KcerFactoryAsh::EnsureFactoryBuilt() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetGlobalPointer()) {
    KcerFactoryAsh* new_factory = new KcerFactoryAsh();
    GetGlobalPointer() = new_factory;
    // This assumes that CrosapiManager is created before the main message loop
    // is running (i.e. before PostMainMessageLoopRun Chrome initialization
    // stage) and postpones the initialization until after that. `new_factory`
    // is never destroyed.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&KcerFactoryAsh::Initialize,
                                  base::Unretained(new_factory)));
  }
}

PrefService* GetActiveUserPrefs() {
  if (!user_manager::UserManager::IsInitialized()) {
    return nullptr;
  }
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  if (!manager) {
    return nullptr;
  }
  user_manager::User* user = manager->GetActiveUser();
  if (!user) {
    return nullptr;
  }
  return user->GetProfilePrefs();
}

void KcerFactoryAsh::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (UseKcerWithoutNss()) {
    StartInitializingDeviceKcerWithoutNss();
  } else {
    StartInitializingDeviceKcerForNss();
  }

  // Check whether prefs for the active user are already available. If yes,
  // continue with the potential rollback, otherwise observe session_controller
  // and wait for the user. In Lacros Chrome is restarted with the correct user
  // instead of adding a new one on user login, so
  // OnActiveUserPrefServiceChanged() is not called.
  PrefService* pref_service = GetActiveUserPrefs();
  if (pref_service) {
    return MaybeScheduleRollbackForCertDoubleWrite(pref_service);
  }
  if (ash::Shell::HasInstance() && ash::Shell::Get()->session_controller()) {
    ash::Shell::Get()->session_controller()->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

void KcerFactoryAsh::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNssChapsDualWrittenCertsExist,
                                /*default_value=*/false);
}

// Writes the prefs::kNssChapsDualWrittenCertsExist pref into the pref storage
// of the currently active user. The value might be used to clean up the user
// slot in Chaps, which is semantically owned by the current ChromeOS user.
void KcerFactoryAsh::RecordPkcs12CertDualWrittenImpl() {
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  if (!manager) {
    return;
  }
  user_manager::User* user = manager->GetActiveUser();
  if (!user) {
    return;
  }
  PrefService* prefs = user->GetProfilePrefs();
  if (!prefs) {
    return;
  }
  prefs->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
}

base::WeakPtr<Kcer> KcerFactoryAsh::GetKcerImpl(Profile* profile) {
  if (ash::IsSigninBrowserContext(profile) ||
      ash::IsLockScreenBrowserContext(profile)) {
    if (ash::switches::IsSigninFrameClientCertsEnabled()) {
      // Sign-in and lock screen profiles should only have access to the device
      // token.
      return ExtraInstances::GetDeviceKcer();
    } else {
      return ExtraInstances::GetEmptyKcer();
    }
  }

  if (ash::IsLockScreenAppBrowserContext(profile)) {
    // Returning an empty Kcer here is not a strict requirement, but seem to be
    // the status quo for now.
    return ExtraInstances::GetEmptyKcer();
  }

  if (ash::IsUserBrowserContext(profile)) {
    return KcerFactory::GetKcerImpl(profile);
  }

  return ExtraInstances::GetEmptyKcer();
}

bool KcerFactoryAsh::IsPrimaryContext(content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ash::ProfileHelper::IsPrimaryProfile(
      Profile::FromBrowserContext(context));
}

void KcerFactoryAsh::StartInitializingKcerWithoutNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  const user_manager::User* user = GetUserByContext(context);
  if (!user) {
    return KcerFactory::InitializeKcerInstanceWithoutNss(
        kcer_service, /*user_token_id=*/std::nullopt,
        /*device_token_id=*/std::nullopt);
  }

  if (user->IsAffiliated()) {
    return GetDeviceTokenInfo(std::move(kcer_service), user->GetAccountId());
  }

  return GetUserTokenInfo(std::move(kcer_service), user->GetAccountId(),
                          /*scoped_device_token_info_getter=*/nullptr,
                          /*device_token_info=*/std::nullopt);
}

void KcerFactoryAsh::GetDeviceTokenInfo(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    AccountId account_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter =
      ash::TPMTokenInfoGetter::CreateForSystemToken(
          ash::CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  ash::TPMTokenInfoGetter* device_token_info_getter =
      scoped_device_token_info_getter.get();

  // Bind `scoped_device_token_info_getter` to the callback to ensure it does
  // not go away before TPM token info is fetched. `Unretained` is safe, the
  // factory is never destroyed.
  device_token_info_getter->Start(
      base::BindOnce(&KcerFactoryAsh::GetUserTokenInfo, base::Unretained(this),
                     std::move(kcer_service), std::move(account_id),
                     std::move(scoped_device_token_info_getter)));
}

void KcerFactoryAsh::GetUserTokenInfo(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    AccountId account_id,
    std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
    std::optional<user_data_auth::TpmTokenInfo> device_token_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter =
      ash::TPMTokenInfoGetter::CreateForUserToken(
          account_id, ash::CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  ash::TPMTokenInfoGetter* user_token_info_getter =
      scoped_user_token_info_getter.get();

  // Bind `scoped_user_token_info_getter` to the callback to ensure it does not
  // go away before TPM token info is fetched. `Unretained` is safe, the factory
  // is never destroyed.
  user_token_info_getter->Start(
      base::BindOnce(&KcerFactoryAsh::GotAllTokenInfos, base::Unretained(this),
                     std::move(kcer_service), std::move(device_token_info),
                     std::move(scoped_user_token_info_getter)));
}

void KcerFactoryAsh::GotAllTokenInfos(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    std::optional<user_data_auth::TpmTokenInfo> device_token_info,
    std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter,
    std::optional<user_data_auth::TpmTokenInfo> user_token_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  std::optional<SessionChapsClient::SlotId> user_token_id;
  if (user_token_info) {
    user_token_id = SessionChapsClient::SlotId(
        static_cast<uint64_t>(user_token_info->slot()));
  }
  std::optional<SessionChapsClient::SlotId> device_token_id;
  if (device_token_info) {
    device_token_id = SessionChapsClient::SlotId(
        static_cast<uint64_t>(device_token_info->slot()));
  }

  KcerFactory::InitializeKcerInstanceWithoutNss(kcer_service, user_token_id,
                                                device_token_id);
}

void KcerFactoryAsh::StartInitializingDeviceKcerForNss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ash::SystemTokenCertDbStorage::Get()) {
    CHECK_IS_TEST();
    return;
  }

  auto initialize_callback_ui = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&KcerFactoryAsh::InitializeDeviceKcerForNss,
                     base::Unretained(this)));
  auto prepare_tokens_on_io = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(GetPrepareTokensForNssOnIOThreadFunctor(),
                     std::move(initialize_callback_ui)));

  // SystemTokenCertDbStorage looks suspicious because it returns the database
  // to the UI thread and not to the IO thread like NssService. For now just
  // forward the database immediately to the IO thread (which is done implicitly
  // by binding `prepare_tokens_on_io` to the IO thread). The "done" callback
  // will return the pointer to the device token to the UI thread.
  ash::SystemTokenCertDbStorage::Get()->GetDatabase(
      std::move(prepare_tokens_on_io));
}

void KcerFactoryAsh::InitializeDeviceKcerForNss(
    base::WeakPtr<internal::KcerToken> /*user_token*/,
    base::WeakPtr<internal::KcerToken> device_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ExtraInstances::Get()->InitializeDeviceKcer(
      content::GetIOThreadTaskRunner({}), std::move(device_token));
}

void KcerFactoryAsh::StartInitializingDeviceKcerWithoutNss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter =
      ash::TPMTokenInfoGetter::CreateForSystemToken(
          ash::CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  ash::TPMTokenInfoGetter* device_token_info_getter =
      scoped_device_token_info_getter.get();

  // Bind |device_token_info_getter| to the callback to ensure it does not go
  // away before TPM token info is fetched. `Unretained` is safe, the factory is
  // never destroyed.
  device_token_info_getter->Start(base::BindOnce(
      &KcerFactoryAsh::InitializeDeviceKcerWithoutNss, base::Unretained(this),
      std::move(scoped_device_token_info_getter)));
}

void KcerFactoryAsh::InitializeDeviceKcerWithoutNss(
    std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
    std::optional<user_data_auth::TpmTokenInfo> device_token_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<SessionChapsClient::SlotId> device_token_id;
  if (device_token_info) {
    device_token_id = SessionChapsClient::SlotId(
        static_cast<uint64_t>(device_token_info->slot()));
  }

  base::WeakPtr<internal::KcerToken> device_token;
  if (device_token_id) {
    device_token =
        KcerFactory::GetTokenWithoutNss(device_token_id, Token::kDevice);
  }

  ExtraInstances::Get()->InitializeDeviceKcer(
      content::GetIOThreadTaskRunner({}), std::move(device_token));
}

bool KcerFactoryAsh::EnsureHighLevelChapsClientInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsHighLevelChapsClientInitialized()) {
    return true;
  }

  session_chaps_client_ = std::make_unique<SessionChapsClientImpl>(
      base::BindRepeating(&GetChapsService));
  high_level_chaps_client_ =
      std::make_unique<HighLevelChapsClientImpl>(session_chaps_client_.get());

  return IsHighLevelChapsClientInitialized();
}

void KcerFactoryAsh::OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  MaybeScheduleRollbackForCertDoubleWrite(pref_service);
}

void KcerFactoryAsh::MaybeScheduleRollbackForCertDoubleWrite(
    PrefService* pref_service) {
  if (rollback_helper_) {
    rollback_helper_.reset();
  }
  if (!pref_service) {
    return;
  }
  EnsureHighLevelChapsClientInitialized();
  if (internal::KcerRollbackHelper::IsChapsRollbackRequired(pref_service)) {
    rollback_helper_ = std::make_unique<internal::KcerRollbackHelper>(
        high_level_chaps_client_.get(), pref_service);

    return rollback_helper_->PerformRollback();
  }
}

}  // namespace kcer
