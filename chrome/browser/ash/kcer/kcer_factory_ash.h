// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_

#include "chrome/browser/chromeos/kcer/kcer_factory.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "components/account_id/account_id.h"

namespace kcer {

class KcerFactoryAsh final : public KcerFactory {
 public:
  static void EnsureFactoryBuilt();

  KcerFactoryAsh() = default;
  ~KcerFactoryAsh() override = default;

 private:
  void Initialize();

  // Implements KcerFactory.
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
      absl::optional<user_data_auth::TpmTokenInfo> device_token_info);
  void GotAllTokenInfos(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      absl::optional<user_data_auth::TpmTokenInfo> device_token_info,
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter,
      absl::optional<user_data_auth::TpmTokenInfo> user_token_info);

  void StartInitializingDeviceKcerForNss();
  void InitializeDeviceKcerForNss(
      base::WeakPtr<internal::KcerToken> /*user_token*/,
      base::WeakPtr<internal::KcerToken> device_token);

  void StartInitializingDeviceKcerWithoutNss();
  void InitializeDeviceKcerWithoutNss(
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_device_token_info_getter,
      absl::optional<user_data_auth::TpmTokenInfo> device_token_info);
};

}  // namespace kcer

#endif  // CHROME_BROWSER_ASH_KCER_KCER_FACTORY_ASH_H_
