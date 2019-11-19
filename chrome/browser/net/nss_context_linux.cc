// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "net/cert/nss_cert_database.h"

namespace {
net::NSSCertDatabase* g_nss_cert_database = NULL;
}  // namespace

net::NSSCertDatabase* GetNSSCertDatabaseForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  // This initialization is not thread safe. This CHECK ensures that this code
  // is only run on a single thread.
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!g_nss_cert_database) {
    // Linux has only a single persistent slot compared to ChromeOS's separate
    // public and private slot.
    // Redirect any slot usage to this persistent slot on Linux.
    crypto::EnsureNSSInit();
    g_nss_cert_database = new net::NSSCertDatabase(
        crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()) /* public slot */,
        crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()) /* private slot */);
  }
  return g_nss_cert_database;
}
