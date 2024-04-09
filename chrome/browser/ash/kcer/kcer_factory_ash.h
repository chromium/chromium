// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_

#include "ash/public/cpp/session/session_observer.h"
#include "chrome/browser/ash/kcer/nssdb_migration/kcer_rollback_helper.h"
#include "chrome/browser/chromeos/kcer/kcer_factory.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "components/account_id/account_id.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace kcer {

class KcerFactoryAsh final : public KcerFactory, ash::SessionObserver {
 public:
  static void EnsureFactoryBuilt();

  static KcerFactory* GetInstance();

  KcerFactoryAsh();
  ~KcerFactoryAsh() override;

 private:
  void Initialize();

  // Implements BrowserContextKeyedServiceFactory.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  // Implements KcerFactory.
  void RecordPkcs12CertDualWrittenImpl() override;
  base::WeakPtr<Kcer> GetKcerImpl(Profile* profile) override;
  bool IsPrimaryContext(content::BrowserContext* context) const override;
  void StartInitializingKcerWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context) override;
  bool EnsureHighLevelChapsClientInitialized() override;

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

  void StartInitializingDeviceKcerForNss();
  void InitializeDeviceKcerForNss(
      base::WeakPtr<internal::KcerToken> /*user_token*/,
      base::WeakPtr<internal::KcerToken> device_token);

  void StartInitializingDeviceKcerWithoutNss();
  void InitializeDeviceKcerWithoutNss(
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
      std::optional<user_data_auth::TpmTokenInfo> device_token_info);
  // Implements users preference service observer.
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void MaybeScheduleRollbackForCertDoubleWrite(PrefService* pref_service);

  std::unique_ptr<internal::KcerRollbackHelper> rollback_helper_;
};

}  // namespace kcer

#endif  // CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
