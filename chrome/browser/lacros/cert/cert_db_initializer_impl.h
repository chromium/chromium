// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IMPL_H_
#define CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IMPL_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lacros/cert/cert_db_initializer.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_io_impl.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "crypto/scoped_nss_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_database.h"

class Profile;

// Initializes the certificate database in Lacros-Chrome for the specified
// profile. This object should only be accessed on the UI thread, and requires
// the Mojo CertDatabase interface to be available on the `LacrosService`.
//
// On Lacros-Chrome, how a database is initialized depends on which profile is
// being used as well as the device capabilities.
//
// - For devices with a dedicated TPM, for the main profile the public slot will
//   be in the current user's cryptohome (a path is provided by Ash) and the
//   private slot will be on the TPM (its slot id is provided by Ash).
//
// - For devices with a dedicated TPM, Ash may also indicate that Lacros should
//   use the system slot stored on the TPM. If indicated, the main profile will
//   attempt to load the system slot (its slot id is provided by Ash). In case
//   of failure, the profile won't have access to certificates from the system
//   slot (it will log an error, but won't crash).
//
// - Devices without a dedicated TPM are currently not fully
//   supported. For the main profile the public slot be in the current user's
//   cryptohome (a path is provided by Ash), the private slot will be set to a
//   read-only slot that does not support modification or permanent objects.
//   TODO(b/197082753): Make the private slot reference the public slot (same as
//   in Ash).
//
// - For all devices, secondary profiles will be initialized with both
//   public and private slots set to read-only slots that do not
//   support modification or permanent objects.
//
// - If Ash provides a path/TPM slot, and Lacros is unable to load
//   it, Lacros will intentionally crash, to avoid ignoring
//   settings from Ash.
//
// - If Ash does not provide a path, this is an unexpected/invalid
//   state, but Lacros will fail into read-only mode.
class CertDbInitializerImpl : public CertDbInitializer,
                              public KeyedService,
                              public crosapi::mojom::AshCertDatabaseObserver {
 public:
  explicit CertDbInitializerImpl(Profile* profile);
  ~CertDbInitializerImpl() override;

  // Starts the initialization. For the main profile the database will be
  // initialized based on the information provided by Ash. Secondary profiles
  // will get a read-only database without any user certificates.
  void Start();

  // CertDbInitializer
  base::CallbackListSubscription WaitUntilReady(
      base::OnceClosure callback) override;
  NssCertDatabaseGetter CreateNssCertDatabaseGetterForIOThread() override;

  // Called when there's a change in certificate database in Ash.
  // Forwards the notification to the CertDatabase.
  void OnCertsChangedInAsh(
      crosapi::mojom::CertDatabaseChangeType change_type) override;

 private:
  void InitializeForMainProfile();

  // Initializes a read-only cert database. It only has access to the built-in
  // certs and doesn't allow any modifications.
  void InitializeReadOnlyCertDb();

  // Queries Ash-Chrome for the user and system slots to use for the profile.
  // Should only be called after the software database has been loaded.
  void DidLoadSoftwareNssDb();

  // Receives information for initializing the main cert database and forwards
  // it to `cert_db_initializer_io_`.
  void OnCertDbInfoReceived(
      crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info);

  // A part of the legacy initialization flow. Triggers legacy initialization in
  // `cert_db_initializer_io_`.
  void OnLegacyCertDbInfoReceived(
      crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info);

  // Receives the signal that initialization is finished and notifies observers
  // about that.
  void OnCertDbInitializationFinished();

  // This class is a `KeyedService` based on the `Profile`. An instance is
  // created together with a new profile and never outlives it.`
  raw_ptr<Profile> profile_ = nullptr;
  bool is_ready_ = false;
  base::OnceClosureList callbacks_;

  // Created on the UI thread, but after that, initialized, accessed, and
  // destroyed exclusively on the IO thread. It is safe to pass unretained
  // pointers to this object into IO thread tasks because the earliest it can be
  // destroyed is in the following task posted from the destructor.
  std::unique_ptr<CertDbInitializerIOImpl> cert_db_initializer_io_;

  // Receives mojo messages from ash-chrome (under Streaming mode).
  mojo::Receiver<crosapi::mojom::AshCertDatabaseObserver> receiver_{this};

  base::WeakPtrFactory<CertDbInitializerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IMPL_H_
