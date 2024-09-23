// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert/cert_db_initializer_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "cert_db_initializer_io_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_nss_types.h"
#include "mojo/public/cpp/bindings/remote.h"

using CrosapiCertDb = crosapi::mojom::CertDatabase;

constexpr int kAddAshCertDatabaseObserverMinVersion =
    CrosapiCertDb::kAddAshCertDatabaseObserverMinVersion;

// This object is split between UI and IO threads:
// * CertDbInitializerImpl - is the outer layer that implements the KeyedService
// interface, implicitly tracks lifetime of its profile, triggers the
// initialization and implements the UI part of the NSSCertDatabaseGetter
// interface;
// * CertDbInitializerIOImpl - is the second (inner) part, it lives on the IO
// thread, manages lifetime of loaded NSS slots, NSSCertDatabase and implements
// the IO part of the NSSCertDatabaseGetter interface.
CertDbInitializerImpl::CertDbInitializerImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  cert_db_initializer_io_ = std::make_unique<CertDbInitializerIOImpl>();
}

CertDbInitializerImpl::~CertDbInitializerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In case the initialization didn't finish, notify waiting observers.
  OnCertDbInitializationFinished();

  content::GetIOThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(cert_db_initializer_io_));
}

void CertDbInitializerImpl::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();

  if (!profile_->IsMainProfile() || !lacros_service ||
      !lacros_service->IsAvailable<CrosapiCertDb>()) {
    // TODO(b/191336028): Implement fully functional and separated certificate
    // database for secondary profiles when NSS library is replaced with
    // something more flexible.
    return InitializeReadOnlyCertDb();
  }

  if (lacros_service->GetInterfaceVersion<CrosapiCertDb>() >=
      kAddAshCertDatabaseObserverMinVersion) {
    lacros_service->GetRemote<CrosapiCertDb>()->AddAshCertDatabaseObserver(
        receiver_.BindNewPipeAndPassRemote());
  }

  InitializeForMainProfile();
}

base::CallbackListSubscription CertDbInitializerImpl::WaitUntilReady(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_ready_) {
    // Even if the database is ready, avoid calling the callback synchronously.
    // We still want to support returning a CallbackListSubscription, so this
    // code goes through callbacks_ in that case too, which will be notified in
    // OnCertDbInitializationFinished.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                       weak_factory_.GetWeakPtr()));
  }

  return callbacks_.Add(std::move(callback));
}

void CertDbInitializerImpl::InitializeReadOnlyCertDb() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto init_database_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CertDbInitializerIOImpl::InitializeReadOnlyNssCertDatabase,
          base::Unretained(cert_db_initializer_io_.get()),
          std::move(init_database_callback)));
}

void CertDbInitializerImpl::InitializeForMainProfile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto software_db_loaded_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CertDbInitializerImpl::DidLoadSoftwareNssDb,
                     weak_factory_.GetWeakPtr()));

  if (chromeos::IsKioskSession()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CertDbInitializerIOImpl::InitReadOnlyPublicSlot,
                       base::Unretained(cert_db_initializer_io_.get()),
                       std::move(software_db_loaded_callback)));
    return;
  }

  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  base::FilePath nss_db_path;
  if (init_params->DefaultPaths()->user_nss_database) {
    nss_db_path = init_params->DefaultPaths()->user_nss_database.value();
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CertDbInitializerIOImpl::LoadSoftwareNssDb,
                                base::Unretained(cert_db_initializer_io_.get()),
                                std::move(nss_db_path),
                                std::move(software_db_loaded_callback)));
}

void CertDbInitializerImpl::DidLoadSoftwareNssDb() {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::CertDatabase>()
      ->GetCertDatabaseInfo(
          base::BindOnce(&CertDbInitializerImpl::OnCertDbInfoReceived,
                         weak_factory_.GetWeakPtr()));
}

void CertDbInitializerImpl::OnCertDbInfoReceived(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto init_database_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CertDbInitializerIOImpl::InitializeNssCertDatabase,
                     base::Unretained(cert_db_initializer_io_.get()),
                     std::move(cert_db_info),
                     std::move(init_database_callback)));
}

void CertDbInitializerImpl::OnCertDbInitializationFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_ready_ = true;
  callbacks_.Notify();
}

NssCertDatabaseGetter
CertDbInitializerImpl::CreateNssCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&CertDbInitializerIOImpl::GetNssCertDatabase,
                        base::Unretained(cert_db_initializer_io_.get()));
}

void CertDbInitializerImpl::OnCertsChangedInAsh(
    crosapi::mojom::CertDatabaseChangeType change_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (change_type) {
    case crosapi::mojom::CertDatabaseChangeType::kUnknown:
      net::CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
      net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
      break;
    case crosapi::mojom::CertDatabaseChangeType::kTrustStore:
      net::CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
      break;
    case crosapi::mojom::CertDatabaseChangeType::kClientCertStore:
      net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
      break;
  }
}
