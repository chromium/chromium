// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_IMPL_H_
#define CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lacros/cert_db_initializer.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class IdentityManagerObserver;

// Initializes certificate database in Lacros-Chrome. Public methods should be
// called from the UI thread. Relies on CertDatabase mojo interface to be
// available.
class CertDbInitializerImpl : public CertDbInitializer, public KeyedService {
 public:
  explicit CertDbInitializerImpl(Profile* profile);
  ~CertDbInitializerImpl() override;

  // Starts the initialization. The first step is to wait for
  // IdentityManager.
  void Start(signin::IdentityManager* identity_manager);

  // CertDbInitializer
  base::CallbackListSubscription WaitUntilReady(
      ReadyCallback callback) override;

 private:
  // It is called when IdentityManager is ready.
  void OnRefreshTokensLoaded();

  // Checks that the current profile is the main profile and, if yes, makes a
  // mojo request to Ash-Chrome to get information about certificate database.
  void WaitForCertDbReady();

  // Checks from the result that the certificate database should be initialized.
  // If yes, loads Chaps and opens user's certificate database.
  void OnCertDbInfoReceived(
      crosapi::mojom::GetCertDatabaseInfoResultPtr result);

  // Recieves initialization result from a worker thread and notifies observers
  // about the result.
  void OnCertDbInitializationFinished(bool is_success);

  // This class is a `KeyedService` based on the `Profile`. An instance is
  // created together with a new profile and never outlives it.`
  Profile* profile_ = nullptr;
  std::unique_ptr<IdentityManagerObserver> identity_manager_observer_;
  base::Optional<bool> is_ready_;
  base::OnceCallbackList<ReadyCallback::RunType> callbacks_;

  base::WeakPtrFactory<CertDbInitializerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_IMPL_H_
