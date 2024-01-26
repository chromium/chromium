// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer/kcer_factory.h"

#include <pk11pub.h>  // For PK11_GetModuleID, PK11_GetSlotID

#include <memory>

#include "base/task/bind_post_task.h"
#include "build/chromeos_buildflags.h"

#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/kcer/extra_instances.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"

namespace kcer {
namespace {

// The singleton holder for the factory, holds either KcerFactoryAsh or
// KcerFactoryLacros. It's initialized together with other keyed service
// factories and never destroyed.
KcerFactory* g_kcer_factory = nullptr;

KcerFactory::UniqueSlotId GetUniqueId(PK11SlotInfo* nss_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return {PK11_GetModuleID(nss_slot), PK11_GetSlotID(nss_slot)};
}

base::WeakPtr<internal::KcerToken> PrepareOneTokenForNss(
    KcerFactory::KcerTokenMapNss* token_map,
    crypto::ScopedPK11Slot nss_slot,
    Token token_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!nss_slot) {
    return nullptr;
  }

  KcerFactory::UniqueSlotId id = GetUniqueId(nss_slot.get());
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
      internal::KcerToken::CreateForNss(token_type);
  new_token->InitializeForNss(std::move(nss_slot));
  base::WeakPtr<internal::KcerToken> result = new_token->GetWeakPtr();
  (*token_map)[id] = std::move(new_token);
  return result;
}

// Finds KcerTokens in `token_map` for the slots from `nss_db` (or creates new
// ones) and returns weak pointers to them through the `callback`.
void PrepareTokensForNss(KcerFactory::KcerTokenMapNss* token_map,
                         KcerFactory::InitializeOnUIThreadCallback callback,
                         net::NSSCertDatabase* nss_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK(token_map);
  if (!nss_db) {
    // Usually should not happen. Maybe possible on shutdown.
    return;
  }

  base::WeakPtr<internal::KcerToken> user_token_ptr =
      PrepareOneTokenForNss(token_map, nss_db->GetPrivateSlot(), Token::kUser);
  base::WeakPtr<internal::KcerToken> device_token_ptr =
      PrepareOneTokenForNss(token_map, nss_db->GetSystemSlot(), Token::kDevice);

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

KcerFactory::KcerService::KcerService(
    std::unique_ptr<internal::KcerImpl> kcer_instance)
    : kcer(std::move(kcer_instance)) {}

KcerFactory::KcerService::~KcerService() = default;

//====================== KcerFactory ===========================================

// static
base::WeakPtr<Kcer> KcerFactory::GetKcer(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(g_kcer_factory);
  return g_kcer_factory->GetKcerImpl(profile);
}

// static
bool KcerFactory::IsHighLevelChapsClientInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(g_kcer_factory);
  // Also check `session_chaps_client_` because it's mandatory dependency.
  return (g_kcer_factory->session_chaps_client_ &&
          g_kcer_factory->high_level_chaps_client_);
}

KcerFactory::KcerFactory()
    : ProfileKeyedServiceFactory(
          "KcerFactory",
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
              // System profiles don't exist in Ash, in Lacros they are used to
              // render the profile selection screen, which shouldn't need keys
              // or certificates.
              .WithSystem(ProfileSelection::kNone)
              // Ash internal profiles are used by ash for the sign-in and lock
              // screens. Not clear if Chrome ever creates incognito profiles
              // for them, but redirect them to the original ones just in case.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(NssServiceFactory::GetInstance());
}

KcerFactory::~KcerFactory() = default;

// static
void KcerFactory::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_kcer_factory && g_kcer_factory->session_chaps_client_) {
    // `session_chaps_client_` is initialized in
    // EnsureHighLevelChapsClientInitialized and should be shut down in Ash and
    // Lacros before its dependencies.
    g_kcer_factory->session_chaps_client_->Shutdown();
  }
}

// static
KcerFactory*& KcerFactory::GetGlobalPointer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_kcer_factory;
}

base::OnceCallback<void(KcerFactory::InitializeOnUIThreadCallback,
                        net::NSSCertDatabase*)>
KcerFactory::GetPrepareTokensForNssOnIOThreadFunctor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&PrepareTokensForNss, &nss_tokens_io_);
}

base::WeakPtr<Kcer> KcerFactory::GetKcerImpl(Profile* profile) {
  content::BrowserContext* context = GetBrowserContextToUse(profile);
  if (!context) {
    return ExtraInstances::GetEmptyKcer();
  }

  KcerService* service = static_cast<KcerService*>(
      GetServiceForBrowserContext(context, /*create=*/true));
  if (!service) {
    return ExtraInstances::GetEmptyKcer();
  }

  return service->kcer->GetWeakPtr();
}

bool KcerFactory::ServiceIsCreatedWithBrowserContext() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(miersh): This should be set to true because Kcer for the primary
  // context needs to be created as soon as possible. It is used by the
  // components through kcer::ExtraInstance::GetDefaultKcer() and on consumer
  // devices to determine whether the current user is the owner. It's disabled
  // for now because Kcer is not used anywhere yet.
  return false;
}

std::unique_ptr<KeyedService>
KcerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto new_kcer = std::make_unique<internal::KcerImpl>();

  // This code assumes that by the time BuildServiceInstanceForBrowserContext is
  // called, the context is initialized enough for IsPrimaryContext() to work
  // correctly.
  if (IsPrimaryContext(context)) {
    ExtraInstances::Get()->SetDefaultKcer(new_kcer->GetWeakPtr());
  }

  // Run StartInitializingKcerInstance asynchronously, so the service is fully
  // created and registered before continuing.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerFactory::StartInitializingKcerInstance,
                     base::Unretained(const_cast<KcerFactory*>(this)),
                     new_kcer->GetWeakPtr(), context));

  return std::make_unique<KcerService>(std::move(new_kcer));
}

void KcerFactory::StartInitializingKcerInstance(
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

void KcerFactory::StartInitializingKcerForNss(
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

  auto nss_db_getter = nss_service->CreateNSSCertDatabaseGetterForIOThread();

  // Unretained is safe, the factory is never destroyed. But the service can be
  // destroyed. Keep a weak pointer to it, so the initialization methods can
  // check whether they need to continue.
  auto initialize_callback_ui = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&KcerFactory::InitializeKcerInstanceForNss,
                     base::Unretained(this), std::move(kcer_service)));

  auto prepare_tokens_on_io = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(GetPrepareTokensForNssOnIOThreadFunctor(),
                     std::move(initialize_callback_ui)));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetNssDbOnIOThread, std::move(nss_db_getter),
                                std::move(prepare_tokens_on_io)));
}

void KcerFactory::InitializeKcerInstanceForNss(
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

void KcerFactory::InitializeKcerInstanceWithoutNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    absl::optional<SessionChapsClient::SlotId> user_token_id,
    absl::optional<SessionChapsClient::SlotId> device_token_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  kcer_service->Initialize(content::GetUIThreadTaskRunner({}),
                           GetTokenWithoutNss(user_token_id, Token::kUser),
                           GetTokenWithoutNss(device_token_id, Token::kDevice));
}

base::WeakPtr<internal::KcerToken> KcerFactory::GetTokenWithoutNss(
    absl::optional<SessionChapsClient::SlotId> token_id,
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

bool KcerFactory::UseKcerWithoutNss() const {
  return base::FeatureList::IsEnabled(kKcerWithoutNss);
}

}  // namespace kcer
