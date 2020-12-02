// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class IdentityManagerObserver;

// Initializes certificate database in Lacros-Chrome.
class CertDbInitializer : public KeyedService {
 public:
  CertDbInitializer(
      mojo::Remote<crosapi::mojom::CertDatabase>& cert_database_remote,
      Profile* profile);
  ~CertDbInitializer() override;

  void Start(signin::IdentityManager* identity_manager);

 private:
  // Starts the initialization. The first step is to wait for IdentityManager.

  void OnRefreshTokensLoaded();

  // Checks that the current profile is the main profile and, if yes, makes a
  // mojo request to Ash-Chrome to get information about certificate database.
  void WaitForCertDbReady();

  // Checks from the result that the certificate database should be initialized.
  // If yes, loads Chaps and opens user's certificate database.
  void OnCertDbInfoReceived(
      crosapi::mojom::GetCertDatabaseInfoResultPtr result);

  mojo::Remote<crosapi::mojom::CertDatabase>& cert_database_remote_;
  Profile* profile_ = nullptr;
  std::unique_ptr<IdentityManagerObserver> identity_manager_observer_;
  base::WeakPtrFactory<CertDbInitializer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_
