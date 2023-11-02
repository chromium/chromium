// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_
#define CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace login_detection {

// Maintains the sites that are saved in password store. These sites will be
// treated as logged-in.
class PasswordStoreSites
    : public password_manager::PasswordStoreInterface::Observer,
      public password_manager::PasswordStoreConsumer {
 public:
  explicit PasswordStoreSites(password_manager::PasswordStoreInterface* store);

  ~PasswordStoreSites() override;

  // Returns whether the site for |url| has credentials saved in this password
  // store.
  bool IsSiteInPasswordStore(const GURL& url) const;

 private:
  // Reads all logins from the password store and starts observing the store for
  //  future changes.
  void DoDeferredInitialization();

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> form_entries)
      override;

  // The password store |this| is observing site entries from.
  raw_ptr<password_manager::PasswordStoreInterface> password_store_;

  // Set of sites saved in the password store. Will be absl::nullopt until the
  // sites are retrieved the fist time.
  absl::optional<std::set<std::string>> password_sites_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PasswordStoreSites> weak_ptr_factory_{this};
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_
