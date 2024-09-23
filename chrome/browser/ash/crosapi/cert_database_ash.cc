// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cert_database_ash.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"

namespace {
using GotDbCallback =
    base::OnceCallback<void(unsigned long private_slot_id,
                            std::optional<unsigned long> system_slot_id)>;

void GotCertDbOnIOThread(GotDbCallback ui_callback,
                         net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(cert_db);

  // Technically `PK11_GetSlotID` returns CK_SLOT_ID, but it is guaranteed by
  // PKCS #11 standard to be unsigned long and it is more convenient to use here
  // because it will be sent through mojo later.
  unsigned long private_slot_id =
      PK11_GetSlotID(cert_db->GetPrivateSlot().get());

  std::optional<unsigned long> system_slot_id;
  crypto::ScopedPK11Slot system_slot = cert_db->GetSystemSlot();
  if (system_slot)
    system_slot_id = PK11_GetSlotID(system_slot.get());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(ui_callback), private_slot_id, system_slot_id));
}

void GetCertDbOnIOThread(GotDbCallback ui_callback,
                         NssCertDatabaseGetter database_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&GotCertDbOnIOThread, std::move(ui_callback)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
}

}  // namespace

namespace crosapi {

CertDatabaseAsh::CertDatabaseAsh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ash::LoginState::IsInitialized());
  ash::LoginState::Get()->AddObserver(this);
}

CertDatabaseAsh::~CertDatabaseAsh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::LoginState::Get()->RemoveObserver(this);
}

void CertDatabaseAsh::BindReceiver(
    mojo::PendingReceiver<mojom::CertDatabase> pending_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  receivers_.Add(this, std::move(pending_receiver));
}

void CertDatabaseAsh::GetCertDatabaseInfo(
    GetCertDatabaseInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40156265): For now Lacros-Chrome will initialize certificate
  // database only in session. Revisit later to decide what to do on the login
  // screen.
  if (!ash::LoginState::Get()->IsUserLoggedIn()) {
    LOG(ERROR) << "Not implemented";
    std::move(callback).Run(nullptr);
    return;
  }

  if (!is_cert_database_ready_.has_value()) {
    WaitForCertDatabaseReady(std::move(callback));
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  // If user is not available or the database was previously attempted to be
  // loaded, and failed, don't retry, just return an empty result that indicates
  // error.
  if (!user || !is_cert_database_ready_.value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Guest users should not have access to certs.
  const bool is_guest = user->GetAccountId() == user_manager::GuestAccountId();

  // Otherwise, if the TPM was already loaded previously, let the
  // caller know.
  mojom::GetCertDatabaseInfoResultPtr result =
      mojom::GetCertDatabaseInfoResult::New();
  result->should_load_chaps = !is_guest && base::SysInfo::IsRunningOnChromeOS();
  result->private_slot_id = private_slot_id_;
  result->enable_system_slot = system_slot_id_.has_value();
  result->system_slot_id =
      result->enable_system_slot ? system_slot_id_.value() : 0;

  std::move(callback).Run(std::move(result));
}

void CertDatabaseAsh::WaitForCertDatabaseReady(
    GetCertDatabaseInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);

  auto got_db_callback =
      base::BindOnce(&CertDatabaseAsh::OnCertDatabaseReady,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCertDbOnIOThread, std::move(got_db_callback),
                     NssServiceFactory::GetForContext(profile)
                         ->CreateNSSCertDatabaseGetterForIOThread()));
}

void CertDatabaseAsh::OnCertDatabaseReady(
    GetCertDatabaseInfoCallback callback,
    unsigned long private_slot_id,
    std::optional<unsigned long> system_slot_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_cert_database_ready_ = true;
  private_slot_id_ = private_slot_id;
  system_slot_id_ = system_slot_id;

  // Calling the initial method again. Since |is_tpm_token_ready_| is not empty
  // this time, it will return some result via mojo.
  GetCertDatabaseInfo(std::move(callback));
}

void CertDatabaseAsh::LoggedInStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Cached result is valid only within one session and should be reset on
  // sign out. Currently it is not necessary to reset it on sign in, but doesn't
  // hurt.
  is_cert_database_ready_.reset();
}

void CertDatabaseAsh::OnCertsChangedInLacros(
    mojom::CertDatabaseChangeType change_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (change_type) {
    case mojom::CertDatabaseChangeType::kUnknown:
      net::CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
      net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
      break;
    case mojom::CertDatabaseChangeType::kTrustStore:
      net::CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
      break;
    case mojom::CertDatabaseChangeType::kClientCertStore:
      net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
      break;
  }
}

void CertDatabaseAsh::AddAshCertDatabaseObserver(
    mojo::PendingRemote<mojom::AshCertDatabaseObserver> observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.Add(
      mojo::Remote<mojom::AshCertDatabaseObserver>(std::move(observer)));
}

void CertDatabaseAsh::SetCertsProvidedByExtension(
    const std::string& extension_id,
    const chromeos::certificate_provider::CertificateInfoList&
        certificate_infos) {
  // Some certificates could've failed to parse, which is represented by
  // nullptr. We ignore such certificates to avoid closing the mojo pipe.
  chromeos::certificate_provider::CertificateInfoList
      filtered_certificate_infos;
  base::ranges::copy_if(certificate_infos,
                        std::back_inserter(filtered_certificate_infos),
                        [&](const auto& certificate_info) {
                          return certificate_info.certificate != nullptr;
                        });
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile);
  certificate_provider_service->SetCertificatesProvidedByExtension(
      extension_id, filtered_certificate_infos);
}

void CertDatabaseAsh::NotifyCertsChangedInAsh(
    mojom::CertDatabaseChangeType change_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& observer : observers_) {
    observer->OnCertsChangedInAsh(change_type);
  }
}

void CertDatabaseAsh::OnPkcs12CertDualWritten() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kcer::KcerFactoryAsh::RecordPkcs12CertDualWritten();
}

}  // namespace crosapi
