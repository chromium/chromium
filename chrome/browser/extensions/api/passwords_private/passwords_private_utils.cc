// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include <tuple>

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
  urls.signon_realm = credential.signon_realm;
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
  if (credential.stored_in.contains(Store::kAccountStore) &&
      credential.stored_in.contains(Store::kProfileStore)) {
    return extensions::api::passwords_private::
        PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT;
  }
  if (credential.stored_in.contains(Store::kAccountStore)) {
    return extensions::api::passwords_private::PASSWORD_STORE_SET_ACCOUNT;
  }
  if (credential.stored_in.contains(Store::kProfileStore)) {
    return extensions::api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  }
  NOTREACHED();
  return extensions::api::passwords_private::PASSWORD_STORE_SET_DEVICE;
}

}  // namespace extensions
