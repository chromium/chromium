// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/fake_nss_service.h"

#include <memory>

#include "chrome/browser/net/nss_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "nss_service.h"

namespace {
net::NSSCertDatabase* NssGetterForIOThread(
    net::NSSCertDatabase* result,
    base::OnceCallback<void(net::NSSCertDatabase*)>) {
  // The check is here because the real NSS getter must also be run on the IO
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return result;
}

std::unique_ptr<KeyedService> CreateService(bool enable_system_slot,
                                            content::BrowserContext* context) {
  return std::make_unique<FakeNssService>(context, enable_system_slot);
}

}  // namespace

// static
FakeNssService* FakeNssService::InitializeForBrowserContext(
    content::BrowserContext* context,
    bool enable_system_slot) {
  KeyedService* service =
      NssServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating(&CreateService, enable_system_slot));
  return static_cast<FakeNssService*>(service);
}

FakeNssService::FakeNssService(content::BrowserContext* context,
                               bool enable_system_slot)
    : NssService(context) {
  public_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();
  private_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();

  auto cert_db = std::make_unique<net::NSSCertDatabaseChromeOS>(
      crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_slot_->slot())),
      crypto::ScopedPK11Slot(PK11_ReferenceSlot(private_slot_->slot())));

  if (enable_system_slot) {
    system_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();
    cert_db->SetSystemSlot(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot_->slot())));
  }
  nss_cert_database_ = std::move(cert_db);
}

FakeNssService::~FakeNssService() {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(nss_cert_database_));
}

NssCertDatabaseGetter FakeNssService::CreateNSSCertDatabaseGetterForIOThread() {
  return base::BindOnce(&NssGetterForIOThread, nss_cert_database_.get());
}

PK11SlotInfo* FakeNssService::GetPublicSlot() const {
  return public_slot_->slot();
}

PK11SlotInfo* FakeNssService::GetPrivateSlot() const {
  return private_slot_->slot();
}

PK11SlotInfo* FakeNssService::GetSystemSlot() const {
  return system_slot_->slot();
}
