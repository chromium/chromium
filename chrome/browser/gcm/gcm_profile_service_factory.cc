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
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
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

GCMProfileServiceFactory::GlobalTestingFactory& GetTestingFactory() {
  static base::NoDestructor<GCMProfileServiceFactory::GlobalTestingFactory>
      testing_factory;
  return *testing_factory;
}

}  // namespace

GCMProfileServiceFactory::ScopedTestingFactoryInstaller::
    ScopedTestingFactoryInstaller(GlobalTestingFactory testing_factory) {
  DCHECK(!GetTestingFactory());
  GetTestingFactory() = std::move(testing_factory);
}

GCMProfileServiceFactory::ScopedTestingFactoryInstaller::
    ~ScopedTestingFactoryInstaller() {
  GetTestingFactory() = GlobalTestingFactory();
}

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop, incognito profiles are checked with IsIncognitoProfile().
  // It's possible for non-incognito profiles to also be off-the-record.
  bool is_profile_supported =
      !Profile::FromBrowserContext(profile)->IsIncognitoProfile();
#else
  bool is_profile_supported = !profile->IsOffTheRecord();
#endif

  // GCM is not supported in incognito mode.
  if (!is_profile_supported) {
    return nullptr;
  }

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
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

GCMProfileServiceFactory::~GCMProfileServiceFactory() {
}

KeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  DCHECK(!profile->IsIncognitoProfile());
#else
  DCHECK(!profile->IsOffTheRecord());
#endif

  if (GlobalTestingFactory& testing_factory = GetTestingFactory()) {
    return testing_factory.Run(context).release();
  }

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
  // TODO(crbug.com/40260641): Removing image fetcher references here breaks
  // tests: org.chromium.chrome.browser.ImageFetcherIntegrationTest Users of
  // image fetcher may be depending on this service to initialize the image
  // fetcher factory. [FATAL:scoped_refptr.h(291)] Check failed: ptr_.
  // ...
  // image_fetcher::GetImageFetcherService()
  ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  return service.release();
}

}  // namespace gcm
