// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_IDENTITY_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_IDENTITY_MANAGER_LACROS_H_

#include "chromeos/crosapi/mojom/identity_manager.mojom.h"

#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"

// This class can be used by lacros to access the identity manager crosapi.
// This API can be used to access properties of the identity manager which lives
// in ash, e.g. to access the name of an account that is not yet known by
// lacros.
class IdentityManagerLacros {
 public:
  IdentityManagerLacros();
  IdentityManagerLacros(const IdentityManagerLacros&) = delete;
  IdentityManagerLacros& operator=(const IdentityManagerLacros&) = delete;
  virtual ~IdentityManagerLacros();

  // Returns a piece of account information of the account with `gaia_id`.
  // If no such account is found, returns an empty value.
  virtual void GetAccountFullName(
      const std::string& gaia_id,
      crosapi::mojom::IdentityManager::GetAccountFullNameCallback callback);
  virtual void GetAccountImage(
      const std::string& gaia_id,
      crosapi::mojom::IdentityManager::GetAccountImageCallback callback);
  virtual void GetAccountEmail(
      const std::string& gaia_id,
      crosapi::mojom::IdentityManager::GetAccountEmailCallback callback);
  virtual void HasAccountWithPersistentError(
      const std::string& gaia_id,
      crosapi::mojom::IdentityManager::HasAccountWithPersistentErrorCallback
          callback);

 private:
  void RunFullNameCallback(
      crosapi::mojom::IdentityManager::GetAccountFullNameCallback callback,
      const std::string& name);
  void RunImageCallback(
      crosapi::mojom::IdentityManager::GetAccountImageCallback callback,
      const gfx::ImageSkia& image);
  void RunEmailCallback(
      crosapi::mojom::IdentityManager::GetAccountEmailCallback callback,
      const std::string& email);
  void RunPersistentErrorCallback(
      crosapi::mojom::IdentityManager::HasAccountWithPersistentErrorCallback
          callback,
      bool persistent_error);

  base::WeakPtrFactory<class IdentityManagerLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_IDENTITY_MANAGER_LACROS_H_
