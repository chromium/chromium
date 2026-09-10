// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include <tuple>

#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using password_manager::CredentialUIEntry;
using Store = password_manager::PasswordForm::Store;

}  // namespace

api::passwords_private::UrlCollection CreateUrlCollectionFromCredential(
    const CredentialUIEntry& credential) {
  api::passwords_private::UrlCollection urls;
  urls.shown = GetShownOrigin(credential);
  urls.link = GetShownUrl(credential).spec();
  urls.signon_realm = credential.GetFirstSignonRealm();
  return urls;
}

api::passwords_private::UrlCollection CreateUrlCollectionFromGURL(
    const GURL& url) {
  api::passwords_private::UrlCollection urls;
  urls.shown = password_manager::GetShownOrigin(url::Origin::Create(url));
  urls.signon_realm = password_manager_util::GetSignonRealm(url);
  urls.link = url.spec();
  return urls;
}

extensions::api::passwords_private::PasswordStoreSet StoreSetFromCredential(
    const CredentialUIEntry& credential) {
  if (!credential.passkey_credential_id.empty()) {
    return extensions::api::passwords_private::PasswordStoreSet::kAccount;
  }
  if (credential.stored_in.contains(Store::kAccountStore) &&
      credential.stored_in.contains(Store::kProfileStore)) {
    return extensions::api::passwords_private::PasswordStoreSet::
        kDeviceAndAccount;
  }
  if (credential.stored_in.contains(Store::kAccountStore)) {
    return extensions::api::passwords_private::PasswordStoreSet::kAccount;
  }
  if (credential.stored_in.contains(Store::kProfileStore)) {
    return extensions::api::passwords_private::PasswordStoreSet::kDevice;
  }
  DUMP_WILL_BE_NOTREACHED();
  return extensions::api::passwords_private::PasswordStoreSet::kDevice;
}

IdGenerator::IdGenerator() = default;
IdGenerator::~IdGenerator() = default;

int IdGenerator::GenerateId(CredentialUIEntry credential) {
  auto iterator =
      key_to_ids_.try_emplace(CreateCredentialSortKey(credential)).first;
  for (int candidate_id : iterator->second) {
    auto credential_iterator = id_to_credential_.find(candidate_id);
    CHECK(credential_iterator != id_to_credential_.end());
    if (credential_iterator->second == credential) {
      // Refresh metadata which does not participate in credential equality.
      credential_iterator->second = std::move(credential);
      return candidate_id;
    }
  }

  iterator->second.push_back(next_id_);
  id_to_credential_.emplace(next_id_, std::move(credential));

  return next_id_++;
}

const CredentialUIEntry* IdGenerator::TryGetKey(int id) const {
  auto it = id_to_credential_.find(id);
  return it != id_to_credential_.end() ? &it->second : nullptr;
}

}  // namespace extensions
