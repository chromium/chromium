// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_
#define CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_

#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace login_detection {

// Maintains the sites that are saved in password store. These sites will be
// treated as logged-in.
class PasswordStoreSites : public password_manager::PasswordStore::Observer,
                           public password_manager::PasswordStoreConsumer {
 public:
  explicit PasswordStoreSites(
      scoped_refptr<password_manager::PasswordStore> store);

  ~PasswordStoreSites() override;

  // Returns whether the site for |url| has credentials saved in this password
  // store.
  bool IsSiteInPasswordStore(const GURL& url) const;

 private:
  // PasswordStore::Observer:
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> form_entries)
      override;

  // The password store |this| is observing site entries from.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // Set of sites saved in the password store. Will be base::nullopt until the
  // sites are retrieved the fist time.
  base::Optional<std::set<std::string>> password_sites_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_PASSWORD_STORE_SITES_H_
