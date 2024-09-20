// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kcer/kcer_factory_ash.h"

#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/extra_instances.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"

namespace kcer {
namespace {

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

KcerFactoryAsh::UniqueSlotId GetUniqueId(PK11SlotInfo* nss_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return {PK11_GetModuleID(nss_slot), PK11_GetSlotID(nss_slot)};
}

base::WeakPtr<internal::KcerToken> PrepareOneTokenForNss(
    KcerFactoryAsh::KcerTokenMapNss* token_map,
    HighLevelChapsClient* chaps_client,
    crypto::ScopedPK11Slot nss_slot,
    Token token_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!nss_slot) {
    return nullptr;
  }

  KcerFactoryAsh::UniqueSlotId id = GetUniqueId(nss_slot.get());
  if (id == GetUniqueId(PK11_GetInternalKeySlot())) {
    // NSSCertDatabase uses the internal slot as a dummy slot in some cases.
    // It's read-only and doesn't contain any certs. Kcer will map the internal
    // slot into a nullptr KcerToken. This can introduce behavioral changes,
    // such as "an empty list of certs is returned" -> "an error is returned",
    // but generally should work correctly.
    return nullptr;
  }

  auto iter = token_map->find(id);
  if (iter != token_map->end()) {
    return iter->second->GetWeakPtr();
  }

  std::unique_ptr<internal::KcerToken> new_token =
      internal::KcerToken::CreateForNss(token_type, chaps_client);
  new_token->InitializeForNss(std::move(nss_slot));
  base::WeakPtr<internal::KcerToken> result = new_token->GetWeakPtr();
  (*token_map)[id] = std::move(new_token);
  return result;
}

// Finds KcerTokens in `token_map` for the slots from `nss_db` (or creates new
// ones) and returns weak pointers to them through the `callback`.
void PrepareTokensForNss(KcerFactoryAsh::KcerTokenMapNss* token_map,
                         HighLevelChapsClient* chaps_client,
                         KcerFactoryAsh::InitializeOnUIThreadCallback callback,
                         net::NSSCertDatabase* nss_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK(token_map);
  if (!nss_db) {
    // Usually should not happen. Maybe possible on shutdown.
    return;
  }

  base::WeakPtr<internal::KcerToken> user_token_ptr = PrepareOneTokenForNss(
      token_map, chaps_client, nss_db->GetPrivateSlot(), Token::kUser);
  base::WeakPtr<internal::KcerToken> device_token_ptr = PrepareOneTokenForNss(
      token_map, chaps_client, nss_db->GetSystemSlot(), Token::kDevice);

  return std::move(callback).Run(std::move(user_token_ptr),
                                 std::move(device_token_ptr));
}

void GetNssDbOnIOThread(
    NssCertDatabaseGetter nss_db_getter,
    base::OnceCallback<void(net::NSSCertDatabase* nss_db)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  net::NSSCertDatabase* cert_db =
      std::move(nss_db_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db) {
    return std::move(split_callback.second).Run(cert_db);
  }
}

}  // namespace

BASE_FEATURE(kKcerWithoutNss,
             "kKcerWithoutNss",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

//====================== KcerService ===========================================

KcerFactoryAsh::KcerService::KcerService(
    std::unique_ptr<internal::KcerImpl> kcer_instance)
    : kcer(std::move(kcer_instance)) {}

KcerFactoryAsh::KcerService::~KcerService() = default;

//====================== KcerFactoryAsh ========================================

// static
base::WeakPtr<Kcer> KcerFactoryAsh::GetKcer(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return GetInstance()->GetKcerImpl(profile);
}

// static
KcerFactoryAsh* KcerFactoryAsh::GetInstance() {
  static base::NoDestructor<KcerFactoryAsh> instance;
  return instance.get();
}

// static
bool KcerFactoryAsh::IsHighLevelChapsClientInitialized() {
  // Also check `session_chaps_client_` because it's mandatory dependency.
  return (GetInstance()->session_chaps_client_ &&
          GetInstance()->high_level_chaps_client_);
}

// static
void KcerFactoryAsh::RecordPkcs12CertDualWritten() {
  GetInstance()->RecordPkcs12CertDualWrittenImpl();
}

// static
void KcerFactoryAsh::ClearNssTokenMapForTesting() {
  GetInstance()->ClearNssTokenMapForTestingImpl();
}

KcerFactoryAsh::KcerFactoryAsh()
    : ProfileKeyedServiceFactory(
          "KcerFactoryAsh",
          // See chrome/browser/profiles/profile_keyed_service_factory.md for
          // descriptions of different profile types.
          ProfileSelections::Builder()
              // Chrome allows using keys and client certificates inside
              // incognito and they are shared with the original profile.
              // Redirect off-the-record profiles to their originals, so a
              // single shared instance of Kcer is created for them.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Guest profiles should behave the same as regular profiles, i.e.
              // normal guest profiles get their own instance of Kcer,
              // off-the-record guest profiles are redirected to their original
              // guest profiles.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // System profiles don't exist in Ash.
              .WithSystem(ProfileSelection::kNone)
              // Ash internal profiles are used by ash for the sign-in and lock
              // screens. All code is expected to use DeviceKcer on these
              // screens (will also be automatically returned from GetKcer()).
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(NssServiceFactory::GetInstance());

  // Unretained is safe, `this` is never destroyed.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerFactoryAsh::Initialize, base::Unretained(this)));
}

KcerFactoryAsh::~KcerFactoryAsh() = default;

void KcerFactoryAsh::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (UseKcerWithoutNss()) {
    StartInitializingDeviceKcerWithoutNss();
  } else {
    StartInitializingDeviceKcerForNss();
  }

  // Check whether prefs for the active user are already available. If yes,
  // continue with the potential rollback, otherwise observe session_controller
  // and wait for the user. If Chrome is restarted with the correct user
  // instead of adding a new one on user login, then
  // OnActiveUserPrefServiceChanged() might not be called.
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

  if (!ash::IsUserBrowserContext(profile)) {
    return ExtraInstances::GetEmptyKcer();
  }

  content::BrowserContext* context = GetBrowserContextToUse(profile);
  if (!context) {
    return ExtraInstances::GetEmptyKcer();
  }
  if (profile->IsSystemProfile()) {
    // System profiles are not expected to exist in Ash, but add a fallback for
    // them just in case.
    return ExtraInstances::GetEmptyKcer();
  }
  if (!ash::IsUserBrowserContext(profile)) {
    // This should be returned for Ash Internal profiles, primarily for the
    // SignIn profile.
    return ExtraInstances::GetDeviceKcer();
  }

  KcerService* service = static_cast<KcerService*>(
      GetServiceForBrowserContext(context, /*create=*/true));
  if (!service) {
    return ExtraInstances::GetEmptyKcer();
  }

  return service->kcer->GetWeakPtr();
}

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

void KcerFactoryAsh::ClearNssTokenMapForTestingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  nss_tokens_io_.clear();
}

bool KcerFactoryAsh::ServiceIsCreatedWithBrowserContext() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This should be true because Kcer for the primary context needs to be
  // created as soon as possible. It is used by the components through
  // kcer::ExtraInstance::GetDefaultKcer() and on consumer devices to determine
  // whether the current user is the owner.
  return true;
}

std::unique_ptr<KeyedService>
KcerFactoryAsh::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto new_kcer = std::make_unique<internal::KcerImpl>();

  // This code assumes that by the time BuildServiceInstanceForBrowserContext is
  // called, the context is initialized enough for IsPrimaryContext() to work
  // correctly.
  if (ash::ProfileHelper::IsPrimaryProfile(
          Profile::FromBrowserContext(context))) {
    ExtraInstances::Get()->SetDefaultKcer(new_kcer->GetWeakPtr());
  }

  // Run StartInitializingKcerInstance asynchronously, so the service is fully
  // created and registered before continuing.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerFactoryAsh::StartInitializingKcerInstance,
                     base::Unretained(const_cast<KcerFactoryAsh*>(this)),
                     new_kcer->GetWeakPtr(),
                     // TODO(crbug.com/40061562): Remove
                     // `UnsafeDanglingUntriaged`
                     base::UnsafeDanglingUntriaged(context)));

  return std::make_unique<KcerService>(std::move(new_kcer));
}

bool KcerFactoryAsh::UseKcerWithoutNss() const {
  return base::FeatureList::IsEnabled(kKcerWithoutNss);
}

void KcerFactoryAsh::StartInitializingKcerInstance(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  if (UseKcerWithoutNss()) {
    return StartInitializingKcerWithoutNss(std::move(kcer_service), context);
  } else {
    return StartInitializingKcerForNss(std::move(kcer_service), context);
  }
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
    return KcerFactoryAsh::InitializeKcerInstanceWithoutNss(
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

  InitializeKcerInstanceWithoutNss(kcer_service, user_token_id,
                                   device_token_id);
}

base::WeakPtr<internal::KcerToken> KcerFactoryAsh::GetTokenWithoutNss(
    std::optional<SessionChapsClient::SlotId> token_id,
    Token token_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!token_id) {
    return nullptr;
  }

  auto iter = chaps_tokens_ui_.find(token_id.value());
  if (iter != chaps_tokens_ui_.end()) {
    return iter->second->GetWeakPtr();
  }

  if (!EnsureHighLevelChapsClientInitialized()) {
    return nullptr;
  }

  std::unique_ptr<internal::KcerToken> new_token =
      internal::KcerToken::CreateWithoutNss(token_type,
                                            high_level_chaps_client_.get());
  new_token->InitializeWithoutNss(token_id.value());
  base::WeakPtr<internal::KcerToken> result = new_token->GetWeakPtr();
  chaps_tokens_ui_[token_id.value()] = std::move(new_token);
  return result;
}

bool KcerFactoryAsh::EnsureHighLevelChapsClientInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsHighLevelChapsClientInitialized()) {
    return true;
  }

  session_chaps_client_ = std::make_unique<SessionChapsClientImpl>();
  high_level_chaps_client_ =
      std::make_unique<HighLevelChapsClientImpl>(session_chaps_client_.get());

  return IsHighLevelChapsClientInitialized();
}

void KcerFactoryAsh::InitializeKcerInstanceWithoutNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    std::optional<SessionChapsClient::SlotId> user_token_id,
    std::optional<SessionChapsClient::SlotId> device_token_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  kcer_service->Initialize(content::GetUIThreadTaskRunner({}),
                           GetTokenWithoutNss(user_token_id, Token::kUser),
                           GetTokenWithoutNss(device_token_id, Token::kDevice));
}

void KcerFactoryAsh::StartInitializingKcerForNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  NssService* nss_service = NssServiceFactory::GetForContext(context);
  if (!nss_service) {
    return InitializeKcerInstanceForNss(std::move(kcer_service), nullptr,
                                        nullptr);
  }

  EnsureHighLevelChapsClientInitialized();

  auto nss_db_getter = nss_service->CreateNSSCertDatabaseGetterForIOThread();

  // Unretained is safe, the factory is never destroyed. But the service can be
  // destroyed. Keep a weak pointer to it, so the initialization methods can
  // check whether they need to continue.
  auto initialize_callback_ui = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&KcerFactoryAsh::InitializeKcerInstanceForNss,
                     base::Unretained(this), std::move(kcer_service)));

  auto prepare_tokens_on_io =
      base::BindOnce(GetPrepareTokensForNssOnIOThreadFunctor(),
                     std::move(initialize_callback_ui));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetNssDbOnIOThread, std::move(nss_db_getter),
                                std::move(prepare_tokens_on_io)));
}

base::OnceCallback<void(KcerFactoryAsh::InitializeOnUIThreadCallback,
                        net::NSSCertDatabase*)>
KcerFactoryAsh::GetPrepareTokensForNssOnIOThreadFunctor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EnsureHighLevelChapsClientInitialized();
  // `high_level_chaps_client_` is never destroyed (as a part of the factory),
  // so it's ok to pass it by a raw pointer. PrepareTokensForNss() will run on
  // the IO thread, but it only needs `high_level_chaps_client_` to pass it
  // further, where it will only be used on the UI thread.
  return base::BindOnce(&PrepareTokensForNss, &nss_tokens_io_,
                        high_level_chaps_client_.get());
}

void KcerFactoryAsh::InitializeKcerInstanceForNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    base::WeakPtr<internal::KcerToken> user_token,
    base::WeakPtr<internal::KcerToken> device_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }
  kcer_service->Initialize(content::GetIOThreadTaskRunner({}), user_token,
                           device_token);
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
    device_token = GetTokenWithoutNss(device_token_id, Token::kDevice);
  }

  ExtraInstances::Get()->InitializeDeviceKcer(
      content::GetIOThreadTaskRunner({}), std::move(device_token));
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
