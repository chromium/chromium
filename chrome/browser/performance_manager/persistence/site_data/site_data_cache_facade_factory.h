// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_

#include "base/auto_reset.h"
#include "base/no_destructor.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace performance_manager {

class SiteDataCacheFacade;
class SiteDataCacheFacadeTest;

// Holds either a SiteDataCacheFactory living on the UI thread (when the
// "RunPerformanceManagerOnMainThread" feature is enabled) or a SequenceBound
// wrapper for a SiteDataCacheFactory living on the PM sequence. Will hold
// absl::monostate (a null type) if no SiteDataCacheFactory exists.
using SiteDataCacheFactoryVariant =
    absl::variant<absl::monostate,
                  SiteDataCacheFactory,
                  base::SequenceBound<SiteDataCacheFactory>>;

// BrowserContextKeyedServiceFactory that adorns each browser context with a
// SiteDataCacheFacade.
//
// There's several components to the SiteDataCache architecture:
//   - SiteDataCacheFacade: A KeyedService living on the UI thread that is used
//     as a facade for a SiteDataCache object living on a separate sequence.
//     There's one instance of this class per profile.
//   - SiteDataCacheFacadeFactory: A KeyedService factory living on the UI
//     thread that adorns each profile with a SiteDataCacheFacade. A counterpart
//     to this class, SiteDataCacheFactory, lives on the same sequence as the
//     SiteDataCache objects to manage their lifetime.
//
// The lifetime of these objects is the following:
//   - At startup, the SiteDataCacheFacadeFactory singleton gets initialized on
//     the UI thread. It creates its SiteDataCacheFactory counterpart living on
//     a separate sequence and wraps it in a base::SequenceBound object to
//     ensure that it only gets used from the appropriate sequence.
//   - When a browser context is created, the SiteDataCacheFacadeFactory object
//     produces a SiteDataCacheFacade for the profile. The creation of this
//     facade causes the creation of a SiteDataCache on the sequence that uses
//     these objects.
//   - When a browser context is destroyed the corresponding SiteDataCacheFacade
//     is destroyed and this also destroys the corresponding SiteDataCache on
//     the proper sequence (via the SequenceBound object).
//   - At shutdown, when the last SiteDataCacheFacade is destroyed, a task is
//     posted to ensure that the SiteDataCacheFactory is destroyed on its
//     sequence.
//
// TODO(crbug.com/40755583): When PerformanceManager moves to the UI thread,
// these facades will no longer be needed. During the transition, when the
// "RunPerformanceManagerOnMainThread" feature is enabled
// SiteDataCacheFacadeFactory holds a direct pointer to the SiteDataCacheFactory
// that also lives on the UI thread, instead of a SequenceBound wrapper as
// described above.
class SiteDataCacheFacadeFactory : public ProfileKeyedServiceFactory {
 public:
  SiteDataCacheFacadeFactory(const SiteDataCacheFacadeFactory&) = delete;
  SiteDataCacheFacadeFactory& operator=(const SiteDataCacheFacadeFactory&) =
      delete;

  ~SiteDataCacheFacadeFactory() override;

  static SiteDataCacheFacadeFactory* GetInstance();

  static std::unique_ptr<base::AutoReset<bool>> EnableForTesting();
  static void DisassociateForTesting(Profile* profile);

  // Returns the SiteDataCacheFacade for `profile` so that it can be directly
  // manipulated in tests.
  SiteDataCacheFacade* GetProfileFacadeForTesting(Profile* profile);

 protected:
  friend class base::NoDestructor<SiteDataCacheFacadeFactory>;
  friend class SiteDataCacheFacade;
  friend class SiteDataCacheFacadeTest;

  SiteDataCacheFacadeFactory();

  SiteDataCacheFactoryVariant& cache_factory() { return cache_factory_; }

  // Should be called early in the creation of a SiteDataCacheFacade to make
  // sure that |cache_factory_| gets created.
  void OnBeforeFacadeCreated(base::PassKey<SiteDataCacheFacade> key);

  // Should be called at the end of the destruction of a SiteDataCacheFacade to
  // release |cache_factory_| if there's no more profile needing it.
  void OnFacadeDestroyed(base::PassKey<SiteDataCacheFacade> key);

 private:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  // The counterpart of this factory living on the SiteDataCache's sequence.
  SiteDataCacheFactoryVariant cache_factory_;

  // The number of SiteDataCacheFacade currently in existence.
  size_t service_instance_count_ = 0;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
