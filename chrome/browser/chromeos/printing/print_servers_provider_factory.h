// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_FACTORY_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class AccountId;
class Profile;

namespace chromeos {

class PrintServersProvider;

// Dispenses PrintServersProvider objects based on account id.  Access to this
// object should be sequenced. All methods are called from UI thread.
class PrintServersProviderFactory {
 public:
  static PrintServersProviderFactory* Get();

  PrintServersProviderFactory();

  // Returns a WeakPtr to the PrintServersProvider registered for
  // |account_id|. If an PrintServersProvider does not exist, one will be
  // created for |account_id|. The returned object remains valid until
  // RemoveForUserId or Shutdown is called.
  base::WeakPtr<PrintServersProvider> GetForAccountId(
      const AccountId& account_id);

  // Returns a WeakPtr to the PrintServersProvider registered for |profile|
  // which could be nullptr if |profile| does not map to a valid AccountId. The
  // returned object remains valid until RemoveForUserId or Shutdown is called.
  base::WeakPtr<PrintServersProvider> GetForProfile(Profile* profile);

  // Deletes the PrintServersProvider registered for |account_id|.
  void RemoveForAccountId(const AccountId& account_id);

  // Tear down all PrintServersProviders.
  void Shutdown();

 private:
  ~PrintServersProviderFactory();

  std::map<AccountId, std::unique_ptr<PrintServersProvider>> providers_by_user_;

  DISALLOW_COPY_AND_ASSIGN(PrintServersProviderFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_FACTORY_H_
