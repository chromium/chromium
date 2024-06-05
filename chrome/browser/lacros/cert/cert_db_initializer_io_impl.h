// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IO_IMPL_H_
#define CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IO_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/nss_cert_database_chromeos.h"

// This class is a part of CertDbInitializerImpl that lives on the IO thread.
// All the methods (except for the constructor) must be called on the IO thread.
// The main purpose of the class is to create NSSCertDatabase and provide
// access to it.
class CertDbInitializerIOImpl : public net::NSSCertDatabase::Observer {
 public:
  using GetNSSCertDatabaseCallback =
      base::OnceCallback<void(net::NSSCertDatabase*)>;

  CertDbInitializerIOImpl();
  ~CertDbInitializerIOImpl() override;

  // If `nss_cert_database_` is already created, returns a pointer to it and
  // never calls the `callback`. Otherwise, returns nullptr and will call the
  // `callback` when it is created.
  net::NSSCertDatabase* GetNssCertDatabase(GetNSSCertDatabaseCallback callback);

  // Similar to LoadSoftwareNssDb(), but will use the internal slot as the
  // public slot instead of loading the software NSS database from disk.
  void InitReadOnlyPublicSlot(base::OnceClosure done_callback);

  // Loads the software NSS certificate database (a.k.a. public slot). This step
  // should be executed as soon as possbile because the database may contain
  // user defined CA trust settings that are required for loading web pages.
  // `load_callback` will be called when the load is done.
  void LoadSoftwareNssDb(const base::FilePath& user_nss_database_path,
                         base::OnceClosure load_callback);

  // Creates an NSSCertDatabase based upon the initialization data from
  // `cert_db_info`. This database will then be used for all waiting and
  // future calls to `GetNssCertDatabase`. If `cert_db_info` is nullptr, a
  // read-only database will be created, without any user certificates.
  // `init_callback` will be called when the initialization is done.
  //
  // Summary of what slots are used and when:
  // Public slot:
  // * It is expected to reference the software NSS database in the user
  // directory.
  // * If the path to the directory was not provided by Ash, it will reference
  // the internal slot.
  // * In case of errors it will be empty which will cause a crash if Chrome
  // tries to read it.
  //
  // Private slot:
  // * It is expected to reference a per-user cert storage in Chaps (i.e. in
  // TPM).
  // * If Chaps should not be loaded for the current session (e.g. for guest
  // sessions) or if it failed to load, the private slot will reference the
  // internal slot.
  //
  // System slot:
  // * If it should be used it is expected to reference the device-wide cert
  // storage in Chaps. The decision is made in Ash-Chrome.
  // * Otherwise or if fails to load, system slot will be empty. This will limit
  // access to some certs, but otherwise won't break Chrome.
  void InitializeNssCertDatabase(
      crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info,
      base::OnceClosure init_callback);

  // Initializes a read-only database without any user certificates.
  // `init_callback` will be called when the initialization is done.
  void InitializeReadOnlyNssCertDatabase(base::OnceClosure init_callback);

  // net::NSSCertDatabase::Observer
  void OnTrustStoreChanged() override;
  void OnClientCertStoreChanged() override;

 private:
  void DidLoadSoftwareNssDb(base::OnceClosure load_callback,
                            crypto::ScopedPK11Slot public_slot);

  void DidLoadSlots(base::OnceClosure init_callback,
                    crypto::ScopedPK11Slot private_slot,
                    crypto::ScopedPK11Slot system_slot);

  crypto::ScopedPK11Slot pending_public_slot_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> nss_cert_database_;
  base::OnceCallbackList<GetNSSCertDatabaseCallback::RunType>
      ready_callback_list_;

  base::WeakPtrFactory<CertDbInitializerIOImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_IO_IMPL_H_
