// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer/kcer_factory_lacros.h"

#include "chrome/browser/chromeos/kcer/kcer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_thread.h"

namespace kcer {

// static
void KcerFactoryLacros::EnsureFactoryBuilt() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetGlobalPointer()) {
    GetGlobalPointer() = new KcerFactoryLacros();
  }
}

bool KcerFactoryLacros::IsPrimaryContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return (context && Profile::FromBrowserContext(context)->IsMainProfile());
}

void KcerFactoryLacros::StartInitializingKcerWithoutNss(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();

  // TODO(b/191336028): For now access to keys and client certificates is only
  // implemented for the main profile.
  if (!IsPrimaryContext(context) || !lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::CertDatabase>()) {
    return KcerFactory::InitializeKcerInstanceWithoutNss(
        kcer_service, /*user_token_id=*/absl::nullopt,
        /*device_token_id=*/absl::nullopt);
  }

  // `Unretained` is safe, the factory is never destroyed.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::CertDatabase>()
      ->GetCertDatabaseInfo(
          base::BindOnce(&KcerFactoryLacros::OnCertDbInfoReceived,
                         base::Unretained(this), std::move(kcer_service)));
}

void KcerFactoryLacros::OnCertDbInfoReceived(
    base::WeakPtr<internal::KcerImpl> kcer_service,
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!kcer_service) {
    return;
  }

  absl::optional<SessionChapsClient::SlotId> user_token_id(
      cert_db_info->private_slot_id);

  absl::optional<SessionChapsClient::SlotId> device_token_id;
  if (cert_db_info->enable_system_slot) {
    device_token_id = SessionChapsClient::SlotId(cert_db_info->system_slot_id);
  }

  KcerFactory::InitializeKcerInstanceWithoutNss(kcer_service, user_token_id,
                                                device_token_id);
}

// This method can in theory fail, but this shouldn't happen. In Lacros, by the
// time this is used in production, the minimal supported version of Ash should
// also always have the interface.
bool KcerFactoryLacros::EnsureHighLevelChapsClientInitialized() {
  if (session_chaps_client_ && high_level_chaps_client_) {
    return true;
  }

  crosapi::mojom::ChapsService* chaps_service = nullptr;
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service && service->IsAvailable<crosapi::mojom::ChapsService>()) {
    chaps_service = service->GetRemote<crosapi::mojom::ChapsService>().get();
  }
  if (!chaps_service) {
    LOG(ERROR) << "ChapsService mojo interface is not available";
    return false;
  }

  session_chaps_client_ =
      std::make_unique<SessionChapsClientImpl>(chaps_service);
  high_level_chaps_client_ =
      std::make_unique<HighLevelChapsClientImpl>(session_chaps_client_.get());

  return (session_chaps_client_ && high_level_chaps_client_);
}

}  // namespace kcer
