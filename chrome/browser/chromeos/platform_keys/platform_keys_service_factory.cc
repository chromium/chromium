// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/cert/nss_cert_database.h"

namespace chromeos {
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
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::RepeatingCallback<void(net::NSSCertDatabase*)> on_got_on_io_thread =
      base::BindRepeating(&DidGetCertDbOnIoThread, origin_task_runner,
                          base::AdaptCallbackForRepeating(std::move(callback)));
  net::NSSCertDatabase* cert_db =
      GetNSSCertDatabaseForResourceContext(context, on_got_on_io_thread);

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
                       base::ThreadTaskRunnerHandle::Get(), std::move(callback),
                       browser_context_->GetResourceContext()));
  }

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            Profile::FromBrowserContext(browser_context_));

    // Use the device-wide system key slot only if the user is affiliated on the
    // device.
    const bool use_system_key_slot = user->IsAffiliated();
    return std::make_unique<ClientCertStoreChromeOS>(
        nullptr,  // no additional provider
        use_system_key_slot, user->username_hash(),
        ClientCertStoreChromeOS::PasswordDelegateFactory());
  }

 private:
  content::BrowserContext* browser_context_;
};

class DelegateForDevice : public PlatformKeysServiceImplDelegate,
                          public SystemTokenCertDBObserver {
 public:
  DelegateForDevice() {
    scoped_observer_.Add(SystemTokenCertDBInitializer::Get());
  }

  ~DelegateForDevice() override = default;

  void GetNSSCertDatabase(OnGotNSSCertDatabase callback) override {
    SystemTokenCertDBInitializer::Get()->GetSystemTokenCertDb(
        std::move(callback));
  }

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return std::make_unique<ClientCertStoreChromeOS>(
        nullptr,  // no additional provider
        /*use_system_key_slot=*/true, /*username_hash=*/std::string(),
        ClientCertStoreChromeOS::PasswordDelegateFactory());
  }

 private:
  ScopedObserver<SystemTokenCertDBInitializer, SystemTokenCertDBObserver>
      scoped_observer_{this};

  // SystemTokenCertDBObserver:
  void OnSystemTokenCertDBDestroyed() override {
    scoped_observer_.RemoveAll();
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
  return device_wide_service_.get();
}

void PlatformKeysServiceFactory::SetDeviceWideServiceForTesting(
    PlatformKeysService* device_wide_service_for_testing) {
  device_wide_service_for_testing_ = device_wide_service_for_testing;
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() = default;

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<PlatformKeysServiceImplDelegate> delegate;
  Profile* profile = Profile::FromBrowserContext(context);
  if (ProfileHelper::IsSigninProfile(profile) ||
      ProfileHelper::IsLockScreenAppProfile(profile)) {
    delegate = std::make_unique<DelegateForDevice>();
  } else {
    delegate = std::make_unique<DelegateForUser>(context);
  }

  return new PlatformKeysServiceImpl(std::move(delegate));
}

content::BrowserContext* PlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace platform_keys
}  // namespace chromeos
