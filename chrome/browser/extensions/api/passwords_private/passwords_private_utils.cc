// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include <tuple>

#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using password_manager::CredentialUIEntry;

}  // namespace

api::passwords_private::UrlCollection CreateUrlCollectionFromCredential(
    const CredentialUIEntry& credential) {
  api::passwords_private::UrlCollection urls;
  urls.shown = GetShownOrigin(credential);
  urls.origin = credential.signon_realm;
  urls.link = GetShownUrl(credential).spec();
  return urls;
}

api::passwords_private::UrlCollection CreateUrlCollectionFromGURL(
    const GURL& url) {
  api::passwords_private::UrlCollection urls;
  urls.shown = password_manager::GetShownOrigin(url::Origin::Create(url));
  urls.origin = password_manager_util::GetSignonRealm(url);
  urls.link = url.spec();
  return urls;
}

}  // namespace extensions
