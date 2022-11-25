// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/net/client_cert_store_ash.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/cert/nss_cert_database.h"

namespace ash {
namespace platform_keys {

namespace {

// Invoked on the IO thread when a NSSCertDatabase is available, delegates back
// to origin thread.
void DidGetCertDbOnIoThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& origin_task_runner,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  origin_task_runner->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), cert_db));
}

// Retrieves the NSSCertDatabase for |context|. Must be called on the IO thread.
void GetCertDatabaseOnIoThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& origin_task_runner,
    PlatformKeysServiceImplDelegate::OnGotNSSCertDatabase callback,
    NssCertDatabaseGetter database_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::RepeatingCallback<void(net::NSSCertDatabase*)> on_got_on_io_thread =
      base::BindRepeating(&DidGetCertDbOnIoThread, origin_task_runner,
                          base::Passed(&callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(on_got_on_io_thread);

  if (cert_db)
    on_got_on_io_thread.Run(cert_db);
}

class DelegateForUser : public PlatformKeysServiceImplDelegate {
 public:
  explicit DelegateForUser(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~DelegateForUser() override = default;

  void GetNSSCertDatabase(OnGotNSSCertDatabase callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetCertDatabaseOnIoThread,
                       base::SingleThreadTaskRunner::GetCurrentDefault(),
                       std::move(callback),
                       NssServiceFactory::GetForContext(browser_context_)
                           ->CreateNSSCertDatabaseGetterForIOThread()));
  }

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    const user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(
        Profile::FromBrowserContext(browser_context_));

    // Use the device-wide system key slot only if the user is affiliated on the
    // device.
    const bool use_system_key_slot = user->IsAffiliated();
    return std::make_unique<ClientCertStoreAsh>(
        nullptr,  // no additional provider
        use_system_key_slot, user->username_hash(),
        ClientCertStoreAsh::PasswordDelegateFactory());
  }

 private:
  content::BrowserContext* browser_context_;
};

class DelegateForDevice : public PlatformKeysServiceImplDelegate,
                          public SystemTokenCertDbStorage::Observer {
 public:
  DelegateForDevice() {
    scoped_observeration_.Observe(SystemTokenCertDbStorage::Get());
  }

  ~DelegateForDevice() override = default;

  void GetNSSCertDatabase(OnGotNSSCertDatabase callback) override {
    SystemTokenCertDbStorage::Get()->GetDatabase(std::move(callback));
  }

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return std::make_unique<ClientCertStoreAsh>(
        nullptr,  // no additional provider
        /*use_system_key_slot=*/true, /*username_hash=*/std::string(),
        ClientCertStoreAsh::PasswordDelegateFactory());
  }

 private:
  base::ScopedObservation<SystemTokenCertDbStorage,
                          SystemTokenCertDbStorage::Observer>
      scoped_observeration_{this};

  // SystemTokenCertDbStorage::Observer
  void OnSystemTokenCertDbDestroyed() override {
    scoped_observeration_.Reset();
    ShutDown();
  }
};

}  // namespace

// static
PlatformKeysService* PlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PlatformKeysServiceFactory* PlatformKeysServiceFactory::GetInstance() {
  return base::Singleton<PlatformKeysServiceFactory>::get();
}

// static
PlatformKeysService* PlatformKeysServiceFactory::GetDeviceWideService() {
  if (device_wide_service_for_testing_)
    return device_wide_service_for_testing_;

  if (!device_wide_service_) {
    device_wide_service_ = std::make_unique<PlatformKeysServiceImpl>(
        std::make_unique<DelegateForDevice>());
  }

  device_wide_service_->SetMapToSoftokenAttrsForTesting(
      map_to_softoken_attrs_for_testing_);

  return device_wide_service_.get();
}

void PlatformKeysServiceFactory::SetDeviceWideServiceForTesting(
    PlatformKeysService* device_wide_service_for_testing) {
  device_wide_service_for_testing_ = device_wide_service_for_testing;
  device_wide_service_for_testing_->SetMapToSoftokenAttrsForTesting(
      map_to_softoken_attrs_for_testing_);
}

void PlatformKeysServiceFactory::SetTestingMode(bool is_testing_mode) {
  map_to_softoken_attrs_for_testing_ = is_testing_mode;
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : ProfileKeyedServiceFactory(
          "PlatformKeysService",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(NssServiceFactory::GetInstance());
}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() = default;

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<PlatformKeysServiceImplDelegate> delegate;
  Profile* profile = Profile::FromBrowserContext(context);
  if (!ProfileHelper::IsUserProfile(profile)) {
    delegate = std::make_unique<DelegateForDevice>();
  } else {
    delegate = std::make_unique<DelegateForUser>(context);
  }

  PlatformKeysServiceImpl* const platform_keys_service_impl =
      new PlatformKeysServiceImpl(std::move(delegate));
  platform_keys_service_impl->SetMapToSoftokenAttrsForTesting(
      map_to_softoken_attrs_for_testing_);

  return platform_keys_service_impl;
}

void PlatformKeysServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  PlatformKeysService* platform_keys_service =
      static_cast<PlatformKeysService*>(
          GetServiceForBrowserContext(context, false));
  if (platform_keys_service) {
    platform_keys_service->SetMapToSoftokenAttrsForTesting(false);
  }

  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

}  // namespace platform_keys
}  // namespace ash
