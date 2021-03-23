// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/password_store_sites.h"

#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "components/password_manager/core/browser/password_form.h"

namespace login_detection {

PasswordStoreSites::PasswordStoreSites(
    scoped_refptr<password_manager::PasswordStore> password_store)
    : password_store_(std::move(password_store)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (password_store_) {
    password_store_->AddObserver(this);
    password_store_->GetAllLogins(this);
  }
}

PasswordStoreSites::~PasswordStoreSites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void PasswordStoreSites::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Fetch the login list again.
  password_store_->GetAllLogins(this);
}

void PasswordStoreSites::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> form_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  password_sites_ = std::set<std::string>();
  for (const auto& entry : form_entries) {
    if (!entry->url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    password_sites_->insert(GetSiteNameForURL(entry->url));
  }
}

bool PasswordStoreSites::IsSiteInPasswordStore(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean("Login.PasswordStoreSites.InitializedBeforeQuery",
                            password_sites_.has_value());
  return password_sites_ && password_sites_->find(GetSiteNameForURL(url)) !=
                                password_sites_->end();
}

}  // namespace login_detection
