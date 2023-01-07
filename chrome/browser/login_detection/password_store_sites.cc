// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/password_store_sites.h"

#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace login_detection {

PasswordStoreSites::PasswordStoreSites(
    password_manager::PasswordStoreInterface* password_store)
    : password_store_(password_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreSites::DoDeferredInitialization,
                                weak_ptr_factory_.GetWeakPtr()));
}

PasswordStoreSites::~PasswordStoreSites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void PasswordStoreSites::DoDeferredInitialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (password_store_) {
    password_store_->AddObserver(this);
    password_store_->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

void PasswordStoreSites::OnLoginsChanged(
    password_manager::PasswordStoreInterface* /*store*/,
    const password_manager::PasswordStoreChangeList& /*changes*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Fetch the login list again.
  password_store_->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordStoreSites::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& retained_passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  password_sites_ = std::set<std::string>();
  for (const auto& entry : retained_passwords) {
    if (!entry.url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    password_sites_->insert(GetSiteNameForURL(entry.url));
  }
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
