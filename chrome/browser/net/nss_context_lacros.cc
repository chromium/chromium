// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/bind.h"
#include "chrome/browser/lacros/cert_db_initializer.h"
#include "chrome/browser/lacros/cert_db_initializer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

NssCertDatabaseGetter CreateNSSCertDatabaseGetter(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return CertDbInitializerFactory::GetForBrowserContext(browser_context)
      ->CreateNssCertDatabaseGetterForIOThread();
}
