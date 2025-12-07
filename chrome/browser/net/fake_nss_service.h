// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FAKE_NSS_SERVICE_H_
#define CHROME_BROWSER_NET_FAKE_NSS_SERVICE_H_

#include <memory>

#include "chrome/browser/net/nss_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"

namespace content {
class BrowserContext;
}

// Fake NssService that maintains its own temporary slots for testing.
// In the current implementation the system slot owned by the service is not
// shared between different profiles (like it's supposed to be in real code),
// this can be implemented when needed.
class FakeNssService : public NssService {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // Creates a new instance of FakeNssService and configures `context` to use
  // it. If `enable_system_slot` is true, `context` will have access to a system
  // slot (currently not shared between different contexts).
  static FakeNssService* InitializeForBrowserContext(
      content::BrowserContext* context,
      bool enable_system_slot);
#else
  // Creates a new instance of FakeNssService and configures `context` to use
  // it.
  static FakeNssService* InitializeForBrowserContext(
      content::BrowserContext* context);
#endif

  FakeNssService(content::BrowserContext* context, bool enable_system_slot);
  ~FakeNssService() override;

  NssCertDatabaseGetter CreateNSSCertDatabaseGetterForIOThread() override;

  PK11SlotInfo* GetPublicSlot() const;
#if BUILDFLAG(IS_CHROMEOS)
  PK11SlotInfo* GetPrivateSlot() const;
  PK11SlotInfo* GetSystemSlot() const;
#endif

 private:
  std::unique_ptr<crypto::ScopedTestNSSDB> public_slot_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<crypto::ScopedTestNSSDB> private_slot_;
  std::unique_ptr<crypto::ScopedTestNSSDB> system_slot_;
#endif

  std::unique_ptr<net::NSSCertDatabase> nss_cert_database_;
};

#endif  // CHROME_BROWSER_NET_FAKE_NSS_SERVICE_H_
