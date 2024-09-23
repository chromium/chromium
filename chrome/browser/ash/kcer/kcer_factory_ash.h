// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_

#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/kcer/nssdb_migration/kcer_rollback_helper.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"

class Profile;
class PrefService;

namespace net {
class NSSCertDatabase;
}
namespace kcer::internal {
class KcerImpl;
}  // namespace kcer::internal
namespace user_prefs {
class PrefRegistrySyncable;
}

namespace kcer {

// Feature flag to toggle between Kcer-over-NSS and Kcer-without-NSS. If
// disabled, Kcer will use the implementation that relies on NSS. If enabled,
// Kcer will talk directly with Chaps without using NSS.
BASE_DECLARE_FEATURE(kKcerWithoutNss);

class KcerFactoryAsh : public ProfileKeyedServiceFactory, ash::SessionObserver {
 public:
  // Public for the implementation of this class.
  using UniqueSlotId = std::pair<SECMODModuleID, CK_SLOT_ID>;
  using KcerTokenMapNss =
      base::flat_map<UniqueSlotId, std::unique_ptr<internal::KcerToken>>;
  using KcerTokenMapWithoutNss =
      base::flat_map<SessionChapsClient::SlotId,
                     std::unique_ptr<internal::KcerToken>>;
  using InitializeOnUIThreadCallback =
      base::OnceCallback<void(base::WeakPtr<internal::KcerToken>,
                              base::WeakPtr<internal::KcerToken>)>;

  // Returns a Kcer instance for the `profile`. The lifetime of the instance is
  // bound to the `profile`. If the pointer is cached, it needs to be checked
  // before it's used.
  static base::WeakPtr<Kcer> GetKcer(Profile* profile);

  static KcerFactoryAsh* GetInstance();

  // Public for Pkcs12Migrator unit tests.
  struct KcerService : public KeyedService {
    explicit KcerService(std::unique_ptr<internal::KcerImpl> kcer_instance);
    ~KcerService() override;

    std::unique_ptr<internal::KcerImpl> kcer;
  };

  // Returns whether HighLevelChapsClient was initialized. The method is mostly
  // needed just for testing.
  static bool IsHighLevelChapsClientInitialized();

  // Creates an entry in user preferences in Ash that a PKCS#12 file was
  // dual-written. This will be used in case a rollback for the related
  // experiment is needed.
  static void RecordPkcs12CertDualWritten();

  // Clears cached NSS state. Useful for unittests using Kcer with a test NSS
  // database, the test should clear the Kcer state when finished to avoid
  // leaking into the next test. This must be called on the IO thread.
  static void ClearNssTokenMapForTesting();

 private:
  friend base::NoDestructor<KcerFactoryAsh>;

  KcerFactoryAsh();
  ~KcerFactoryAsh() override;

  // BrowserContextKeyedServiceFactory:
  // The services need to be created immediately after the browser context
  // because the instance for the main profile will also be exposed as the
  // default one and it won't be able to be created on demand from there.
  bool ServiceIsCreatedWithBrowserContext() const override;
  // Creates a new service, which is not fully initialized yet, but can already
  // be used as if it's initialized. Posts a task to finish the initialization.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Implements BrowserContextKeyedServiceFactory.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  // Returns whether the Kcer-without-NSS experiment is enabled.
  bool UseKcerWithoutNss() const;

  void Initialize();

  void StartInitializingKcerInstance(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context);

  void StartInitializingKcerWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context);
  void GetDeviceTokenInfo(base::WeakPtr<internal::KcerImpl> kcer_service,
                          AccountId account_id);
  void GetUserTokenInfo(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      AccountId account_id,
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
      std::optional<user_data_auth::TpmTokenInfo> device_token_info);
  void GotAllTokenInfos(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      std::optional<user_data_auth::TpmTokenInfo> device_token_info,
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter,
      std::optional<user_data_auth::TpmTokenInfo> user_token_info);
  base::WeakPtr<internal::KcerToken> GetTokenWithoutNss(
      std::optional<SessionChapsClient::SlotId> token_id,
      Token token_type);
  bool EnsureHighLevelChapsClientInitialized();
  void InitializeKcerInstanceWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      std::optional<SessionChapsClient::SlotId> user_token_id,
      std::optional<SessionChapsClient::SlotId> device_token_id);

  void StartInitializingKcerForNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context);
  // Returns a functor that will create KcerTokens for each slot from `nss_db`
  // (if necessary) and return weak pointers to them to the provided `callback`.
  // The functor should be run on the IO thread, the callback will be run on the
  // UI thread.
  base::OnceCallback<void(InitializeOnUIThreadCallback callback,
                          net::NSSCertDatabase* nss_db)>
  GetPrepareTokensForNssOnIOThreadFunctor();
  void InitializeKcerInstanceForNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      base::WeakPtr<internal::KcerToken> user_token,
      base::WeakPtr<internal::KcerToken> device_token);

  void StartInitializingDeviceKcerWithoutNss();
  void InitializeDeviceKcerWithoutNss(
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
      std::optional<user_data_auth::TpmTokenInfo> device_token_info);

  void StartInitializingDeviceKcerForNss();
  void InitializeDeviceKcerForNss(
      base::WeakPtr<internal::KcerToken> /*user_token*/,
      base::WeakPtr<internal::KcerToken> device_token);

  base::WeakPtr<Kcer> GetKcerImpl(Profile* profile);
  void RecordPkcs12CertDualWrittenImpl();
  void ClearNssTokenMapForTestingImpl();

  // Implements ash::SessionObserver.
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  // Might schedule a rollback of certs that were double-written into Chaps and
  // NSS (i.e. the deletion of related certs in Chaps).
  void MaybeScheduleRollbackForCertDoubleWrite(PrefService* pref_service);

  // Used by `high_level_chaps_client_` and must outlive it.
  std::unique_ptr<SessionChapsClient> session_chaps_client_;
  // Used by tokens in `chaps_tokens_ui_` (to communicate with Chaps) and must
  // outlive them.
  std::unique_ptr<HighLevelChapsClient> high_level_chaps_client_;

  std::unique_ptr<internal::KcerRollbackHelper> rollback_helper_;

  // Stores the mapping between NSS slots and KcerToken-s. Each private or
  // system slot is mapped into a KcerToken, so a Kcer instance equivalent to a
  // given NSSCertDatabase can be created. Public slots are ignored because
  // there should be no keys or client certificates there by the time Kcer is
  // launched.
  // The map is created on the UI thread, only used on the IO thread and is
  // never destroyed (as a part of NoDestructor<> factory).
  KcerTokenMapNss nss_tokens_io_;
  // Stores the mapping between Chaps tokens and KcerToken-s. The map is created
  // on the UI thread, only used on the UI thread and is never destroyed (as a
  // part of NoDestructor<> factory).
  // Only one token map is used for each Chromium launch, which is controlled by
  // an experiment.
  KcerTokenMapWithoutNss chaps_tokens_ui_;
};

}  // namespace kcer

#endif  // CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
