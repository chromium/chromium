// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include <tuple>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "url/gurl.h"

namespace extensions {

api::passwords_private::UrlCollection CreateUrlCollectionFromForm(
    const password_manager::PasswordForm& form) {
  api::passwords_private::UrlCollection urls;
  GURL link_url;
  std::tie(urls.shown, link_url) =
      password_manager::GetShownOriginAndLinkUrl(form);
  urls.origin = form.signon_realm;
  urls.link = link_url.spec();
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
