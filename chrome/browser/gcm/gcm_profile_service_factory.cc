// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/common/channel_info.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#endif

namespace gcm {

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Requests a ProxyResolvingSocketFactory on the UI thread. Note that a WeakPtr
// of GCMProfileService is needed to detect when the KeyedService shuts down,
// and avoid calling into |profile| which might have also been destroyed.
void RequestProxyResolvingSocketFactoryOnUIThread(
    base::WeakPtr<Profile> profile,
    base::WeakPtr<GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service || !profile)
    return;
  network::mojom::NetworkContext* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

// A thread-safe wrapper to request a ProxyResolvingSocketFactory.
void RequestProxyResolvingSocketFactory(
    base::WeakPtr<Profile> profile,
    base::WeakPtr<GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                std::move(profile), std::move(service),
                                std::move(receiver)));
}
#endif

BrowserContextKeyedServiceFactory::TestingFactory& GetTestingFactory() {
  static base::NoDestructor<BrowserContextKeyedServiceFactory::TestingFactory>
      testing_factory;
  return *testing_factory;
}

}  // namespace

GCMProfileServiceFactory::ScopedTestingFactoryInstaller::
    ScopedTestingFactoryInstaller(TestingFactory testing_factory) {
  DCHECK(!GetTestingFactory());
  GetTestingFactory() = std::move(testing_factory);
}

GCMProfileServiceFactory::ScopedTestingFactoryInstaller::
    ~ScopedTestingFactoryInstaller() {
  GetTestingFactory() = BrowserContextKeyedServiceFactory::TestingFactory();
}

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // GCM is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<GCMProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GCMProfileServiceFactory* GCMProfileServiceFactory::GetInstance() {
  static base::NoDestructor<GCMProfileServiceFactory> instance;
  return instance.get();
}

GCMProfileServiceFactory::GCMProfileServiceFactory()
    : ProfileKeyedServiceFactory(
          "GCMProfileService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  DependsOn(offline_pages::PrefetchServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

GCMProfileServiceFactory::~GCMProfileServiceFactory() {
}

KeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());

  TestingFactory& testing_factory = GetTestingFactory();
  if (testing_factory)
    return testing_factory.Run(context).release();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  std::unique_ptr<GCMProfileService> service;
#if BUILDFLAG(IS_ANDROID)
  service = std::make_unique<GCMProfileService>(profile->GetPath(),
                                                blocking_task_runner);
#else
  service = std::make_unique<GCMProfileService>(
      profile->GetPrefs(), profile->GetPath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory,
                          profile->GetWeakPtr()),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      content::GetNetworkConnectionTracker(), chrome::GetChannel(),
      gcm::GetProductCategoryForSubtypes(profile->GetPrefs()),
      IdentityManagerFactory::GetForProfile(profile),
      std::make_unique<GCMClientFactory>(), content::GetUIThreadTaskRunner({}),
      content::GetIOThreadTaskRunner({}), blocking_task_runner);
#endif
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // TODO(crbug/1424920): PrefetchService is being removed. Leave this for at
  // least one milestone.
  offline_pages::PrefetchServiceFactory::GetForKey(profile->GetProfileKey());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  return service.release();
}

}  // namespace gcm
