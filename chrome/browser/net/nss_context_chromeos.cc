// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database_chromeos.h"

namespace {

void* kDatabaseManagerKey = &kDatabaseManagerKey;
void* kUsernameHashKey = &kUsernameHashKey;

class NSSUsernameHash : public base::SupportsUserData::Data {
 public:
  explicit NSSUsernameHash(const std::string& username_hash)
      : username_hash_(username_hash) {}
  NSSUsernameHash(const NSSUsernameHash&) = delete;
  NSSUsernameHash& operator=(const NSSUsernameHash&) = delete;
  ~NSSUsernameHash() override = default;

  const std::string& username_hash() { return username_hash_; }

 private:
  const std::string username_hash_;
};

class NSSCertDatabaseChromeOSManager : public base::SupportsUserData::Data {
 public:
  typedef base::OnceCallback<void(net::NSSCertDatabaseChromeOS*)>
      GetNSSCertDatabaseCallback;
  explicit NSSCertDatabaseChromeOSManager(const std::string& username_hash)
      : username_hash_(username_hash) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    crypto::ScopedPK11Slot private_slot(crypto::GetPrivateSlotForChromeOSUser(
        username_hash,
        base::BindOnce(&NSSCertDatabaseChromeOSManager::DidGetPrivateSlot,
                       weak_ptr_factory_.GetWeakPtr())));
    if (private_slot)
      DidGetPrivateSlot(std::move(private_slot));
  }

  ~NSSCertDatabaseChromeOSManager() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }

  net::NSSCertDatabaseChromeOS* GetNSSCertDatabase(
      GetNSSCertDatabaseCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (nss_cert_database_)
      return nss_cert_database_.get();

    ready_callback_list_.AddUnsafe(std::move(callback));
    return NULL;
  }

 private:
  using ReadyCallbackList =
      base::OnceCallbackList<GetNSSCertDatabaseCallback::RunType>;

  void DidGetPrivateSlot(crypto::ScopedPK11Slot private_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    nss_cert_database_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(username_hash_),
        std::move(private_slot)));

    ready_callback_list_.Notify(nss_cert_database_.get());
  }

  std::string username_hash_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> nss_cert_database_;
  ReadyCallbackList ready_callback_list_;
  base::WeakPtrFactory<NSSCertDatabaseChromeOSManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NSSCertDatabaseChromeOSManager);
};

net::NSSCertDatabaseChromeOS* GetNSSCertDatabaseChromeOS(
    content::ResourceContext* context,
    NSSCertDatabaseChromeOSManager::GetNSSCertDatabaseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NSSCertDatabaseChromeOSManager* manager =
      static_cast<NSSCertDatabaseChromeOSManager*>(
          context->GetUserData(kDatabaseManagerKey));
  if (!manager) {
    NSSUsernameHash* username_hash =
        static_cast<NSSUsernameHash*>(context->GetUserData(kUsernameHashKey));
    // |username_hash| should not be null here, but handle it just in case it
    // is, to avoid a crash.
    // TODO(https://cbug.com/1018972): This check will be removed as part of
    // removing the ResourceContext dependency of this code.
    manager = new NSSCertDatabaseChromeOSManager(
        username_hash ? username_hash->username_hash() : std::string());
    context->SetUserData(kDatabaseManagerKey, base::WrapUnique(manager));
  }
  return manager->GetNSSCertDatabase(std::move(callback));
}

void CallWithNSSCertDatabase(
    base::OnceCallback<void(net::NSSCertDatabase*)> callback,
    net::NSSCertDatabaseChromeOS* db) {
  std::move(callback).Run(db);
}

void SetSystemSlot(crypto::ScopedPK11Slot system_slot,
                   net::NSSCertDatabaseChromeOS* db) {
  db->SetSystemSlot(std::move(system_slot));
}

void SetSystemSlotOfDBForResourceContext(content::ResourceContext* context,
                                         crypto::ScopedPK11Slot system_slot) {
  base::RepeatingCallback<void(net::NSSCertDatabaseChromeOS*)> callback =
      base::BindRepeating(&SetSystemSlot, base::Passed(&system_slot));

  net::NSSCertDatabaseChromeOS* db =
      GetNSSCertDatabaseChromeOS(context, callback);
  if (db)
    callback.Run(db);
}

net::NSSCertDatabase* GetNSSCertDatabaseForResourceContext(
    content::ResourceContext* context,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  return GetNSSCertDatabaseChromeOS(
      context, base::BindOnce(&CallWithNSSCertDatabase, std::move(callback)));
}

}  // namespace

NssCertDatabaseGetter CreateNSSCertDatabaseGetter(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context);
  return base::BindOnce(
      &GetNSSCertDatabaseForResourceContext,
      base::Unretained(browser_context->GetResourceContext()));
}

void SetNSSCertDatabaseUsernameHash(content::ResourceContext* context,
                                    const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DCHECK(!context->GetUserData(kUsernameHashKey));
  context->SetUserData(kUsernameHashKey,
                       std::make_unique<NSSUsernameHash>(username_hash));
}

void EnableNSSSystemKeySlotForResourceContext(
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::RepeatingCallback<void(crypto::ScopedPK11Slot)> callback =
      base::BindRepeating(&SetSystemSlotOfDBForResourceContext, context);
  crypto::ScopedPK11Slot system_slot = crypto::GetSystemNSSKeySlot(callback);
  if (system_slot)
    callback.Run(std::move(system_slot));
}
