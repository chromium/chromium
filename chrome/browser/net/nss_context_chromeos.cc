// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/bind.h"
#include "chrome/browser/net/nss_service_chromeos.h"
#include "chrome/browser/net/nss_service_chromeos_factory.h"
#include "content/public/browser/browser_thread.h"

NssCertDatabaseGetter CreateNSSCertDatabaseGetter(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return NssServiceChromeOSFactory::GetForContext(browser_context)
      ->CreateNSSCertDatabaseGetterForIOThread();
}
