// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer.h"

#include "base/callback_forward.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "mojo/public/cpp/bindings/remote.h"

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

CertDbInitializer::CertDbInitializer(
    mojo::Remote<crosapi::mojom::CertDatabase>& cert_database_remote,
    Profile* profile)
    : cert_database_remote_(cert_database_remote), profile_(profile) {}

CertDbInitializer::~CertDbInitializer() = default;

void CertDbInitializer::Start(signin::IdentityManager* identity_manager) {
  DCHECK(identity_manager);
  // TODO(crbug.com/1148300): This is temporary. Until ~2021
  // `Profile::IsMainProfile()` method can return a false negative response if
  // called before refresh tokens are loaded. This should be removed together
  // with the entire usage of `IdentityManager` in this class when it is not the
  // case anymore.
  if (!identity_manager->AreRefreshTokensLoaded()) {
    identity_manager_observer_ =
        std::make_unique<IdentityManagerObserver>(identity_manager);
    identity_manager_observer_->WaitForRefreshTokensLoaded(base::BindOnce(
        &CertDbInitializer::OnRefreshTokensLoaded, weak_factory_.GetWeakPtr()));
    return;
  }
  WaitForCertDbReady();
}

void CertDbInitializer::OnRefreshTokensLoaded() {
  identity_manager_observer_.reset();
  WaitForCertDbReady();
}

void CertDbInitializer::WaitForCertDbReady() {
  if (!profile_->IsMainProfile()) {
    return;
  }

  cert_database_remote_->GetCertDatabaseInfo(base::BindOnce(
      &CertDbInitializer::OnCertDbInfoReceived, weak_factory_.GetWeakPtr()));
}

void CertDbInitializer::OnCertDbInfoReceived(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  if (!cert_db_info) {
    LOG(WARNING) << "Certificate database is not accesible";
    return;
  }

  crypto::EnsureNSSInit();

  // There's no need to save the result of `LoadChaps()`, chaps will stay loaded
  // and can be used anyway. Using the result only to determine success/failure.
  if (cert_db_info->should_load_chaps && !crypto::LoadChaps()) {
    LOG(ERROR) << "Failed to load chaps.";
    return;
  }

  // The slot doesn't need to be saved either. `description` doesn't affect
  // anything. `software_nss_db_path` file path should be already created by
  // Ash-Chrome.
  base::FilePath software_nss_db_path(cert_db_info->software_nss_db_path);
  auto slot = crypto::OpenSoftwareNSSDB(software_nss_db_path,
                                        /*description=*/"cert_db");
  if (!slot) {
    LOG(ERROR) << "Failed to open user certificate database";
  }
}
