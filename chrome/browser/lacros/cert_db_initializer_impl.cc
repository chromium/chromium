// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_impl.h"

#include "base/callback_forward.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

bool InitializeCertDbOnWorkerThread(bool should_load_chaps,
                                    base::FilePath software_nss_db_path) {
  crypto::EnsureNSSInit();

  if (should_load_chaps) {
    // NSS functions may reenter //net via extension hooks. If the reentered
    // code needs to synchronously wait for a task to run but the thread pool in
    // which that task must run doesn't have enough threads to schedule it, a
    // deadlock occurs. To prevent that, the base::ScopedBlockingCall below
    // increments the thread pool capacity for the duration of the TPM
    // initialization.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    // There's no need to save the result of `LoadChaps()`, chaps will stay
    // loaded and can be used anyway. Using the result only to determine
    // success/failure.
    if (!crypto::LoadChaps()) {
      LOG(ERROR) << "Failed to load chaps.";
      return false;
    }
  }

  // The slot doesn't need to be saved either. `description` doesn't affect
  // anything. `software_nss_db_path` file path should be already created by
  // Ash-Chrome.
  auto slot = crypto::OpenSoftwareNSSDB(software_nss_db_path,
                                        /*description=*/"cert_db");
  if (!slot) {
    LOG(ERROR) << "Failed to open user certificate database";
    return false;
  }

  return true;
}

}  // namespace

class IdentityManagerObserver : public signin::IdentityManager::Observer {
 public:
  explicit IdentityManagerObserver(signin::IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    DCHECK(identity_manager_);
    identity_manager_->AddObserver(this);
  }

  IdentityManagerObserver(const IdentityManagerObserver&) = delete;
  IdentityManagerObserver& operator==(const IdentityManagerObserver&) = delete;

  ~IdentityManagerObserver() override {
    identity_manager_->RemoveObserver(this);
  }

  void WaitForRefreshTokensLoaded(base::OnceClosure callback) {
    DCHECK(callback_.is_null());
    callback_ = std::move(callback);
  }

 private:
  void OnRefreshTokensLoaded() override {
    if (!callback_) {
      return;
    }
    std::move(callback_).Run();  // NOTE: may delete `this`.
  }

  signin::IdentityManager* identity_manager_ = nullptr;
  base::OnceClosure callback_;
};

// =============================================================================

CertDbInitializerImpl::CertDbInitializerImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(chromeos::LacrosChromeServiceImpl::Get()
             ->IsAvailable<crosapi::mojom::CertDatabase>());
}

CertDbInitializerImpl::~CertDbInitializerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In case the initialization didn't finish, notify waiting observers.
  OnCertDbInitializationFinished(false);
}

void CertDbInitializerImpl::Start(signin::IdentityManager* identity_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(identity_manager);
  // TODO(crbug.com/1148300): This is temporary. Until ~2021
  // `Profile::IsMainProfile()` method can return a false negative response if
  // called before refresh tokens are loaded. This should be removed together
  // with the entire usage of `IdentityManager` in this class when it is not the
  // case anymore.
  if (!identity_manager->AreRefreshTokensLoaded()) {
    identity_manager_observer_ =
        std::make_unique<IdentityManagerObserver>(identity_manager);
    identity_manager_observer_->WaitForRefreshTokensLoaded(
        base::BindOnce(&CertDbInitializerImpl::OnRefreshTokensLoaded,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  WaitForCertDbReady();
}

base::CallbackListSubscription CertDbInitializerImpl::WaitUntilReady(
    ReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_ready_.has_value()) {
    std::move(callback).Run(is_ready_.value());
    return {};
  }

  return callbacks_.Add(std::move(callback));
}

void CertDbInitializerImpl::OnRefreshTokensLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  identity_manager_observer_.reset();
  WaitForCertDbReady();
}

void CertDbInitializerImpl::WaitForCertDbReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!profile_->IsMainProfile()) {
    OnCertDbInitializationFinished(false);
    return;
  }

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::CertDatabase>()
      ->GetCertDatabaseInfo(
          base::BindOnce(&CertDbInitializerImpl::OnCertDbInfoReceived,
                         weak_factory_.GetWeakPtr()));
}

void CertDbInitializerImpl::OnCertDbInfoReceived(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!cert_db_info) {
    LOG(WARNING) << "Certificate database is not accesible";
    OnCertDbInitializationFinished(false);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&InitializeCertDbOnWorkerThread,
                     cert_db_info->should_load_chaps,
                     base::FilePath(cert_db_info->software_nss_db_path)),
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));
}

void CertDbInitializerImpl::OnCertDbInitializationFinished(bool is_success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.Notify(is_success);
  is_ready_ = is_success;
}
