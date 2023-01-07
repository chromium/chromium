// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service.h"

#include <pk11pub.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "net/cert/nss_cert_database.h"

namespace {

net::NSSCertDatabase* GetNSSCertDatabase(
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  // This initialization is not thread safe. This CHECK ensures that this code
  // is only run on a single thread.
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // Linux has only a single persistent slot compared to ChromeOS's separate
  // public and private slot.
  // Redirect any slot usage to this persistent slot on Linux.
  crypto::EnsureNSSInit();
  static base::NoDestructor<net::NSSCertDatabase> g_cert_database{
      crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()) /* public slot */,
      crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()) /* private slot */
  };
  return g_cert_database.get();
}

}  // namespace

NssService::NssService(content::BrowserContext*) {}

NssService::~NssService() = default;

NssCertDatabaseGetter NssService::CreateNSSCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&GetNSSCertDatabase);
}
