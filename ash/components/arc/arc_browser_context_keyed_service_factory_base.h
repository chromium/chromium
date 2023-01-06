// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ARC_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_BASE_H_
#define ASH_COMPONENTS_ARC_ARC_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_BASE_H_

#include <memory>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {
namespace internal {

// Implementation of BrowserContextKeyedServiceFactory for ARC related
// services. The ARC related BrowserContextKeyedService can make its factory
// class derived from this class.
//
// How to use:
// In .h:
// #include "components/keyed_service/core/keyed_service.h"
//
// namespace content {
// class BrowserContext;
// }  // namespace content
//
// class ArcFooService : public KeyedService, ... {
//  public:
//   static ArcFooService* GetForBrowserContext(
//       content::BrowserContext* context);
//
//   ArcFooService(content::BrowserContext* context,
//                 ArcBridgeService* arc_bridge_service);
// };
//
// In .cc:
//
// namespace {
// class ArcFooServiceFactory
//     : public internal::ArcBrowserContextKeyedServiceFactoryBase<
//           ArcFooService,
//           ArcFooServiceFactory> {
//  public:
//   static constexpr const char* kName = "ArcFooServiceFactory";
//
//   static ArcFooServiceFactory* GetInstance() {
//     return base::Singleton<ArcFooServiceFactory>::get();
//   }
//
//  private:
//   friend struct base::DefaultSingletonTraits<ArcFooServiceFactory>;
//   ArcFooServiceFactory() = default;
//   ~ArcFooServiceFactory() override = default;
// };
// }  // namespace
//
// ArcFooService* ArcFooService::GetForBrowserContext(
//     content::BrowserContext* context) {
//   return ArcFooServiceFactory::GetForBrowserContext(context);
// }
//
// If the service depends on other KeyedService, DependsOn() can be called in
// the factory's ctor similar to other BrowserContextKeyedServiceFactory
// subclasses.
//
// This header is intended to be included only from the .cc file directly.
//
// TODO(hidehiko): Make ArcFooService constructor (and maybe destructor)
// private with declaring appropriate friend.
template <typename Service, typename Factory>
class ArcBrowserContextKeyedServiceFactoryBase
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of the service for the given |context|,
  // or nullptr if |context| is not allowed to use ARC.
  // If the instance is not yet created, this creates a new service instance.
  static Service* GetForBrowserContext(content::BrowserContext* context) {
    return static_cast<Service*>(
        Factory::GetInstance()->GetServiceForBrowserContext(context,
                                                            true /* create */));
  }

  // Does the same as GetForBrowserContext() but for testing. This should be
  // called from the |Service|'s unit test's fixture to instantiate the service.
  // Note that this function does not check whether or not |context| is for the
  // primary user. Also, ArcServiceManager has to be instantiated before calling
  // this function.
  static Service* GetForBrowserContextForTesting(
      content::BrowserContext* context) {
    Factory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<Service>(
              context, ArcServiceManager::Get()->arc_bridge_service());
        }));
    return GetForBrowserContext(context);
  }

  ArcBrowserContextKeyedServiceFactoryBase(
      const ArcBrowserContextKeyedServiceFactoryBase&) = delete;
  ArcBrowserContextKeyedServiceFactoryBase& operator=(
      const ArcBrowserContextKeyedServiceFactoryBase&) = delete;

 protected:
  ArcBrowserContextKeyedServiceFactoryBase()
      : BrowserContextKeyedServiceFactory(
            Factory::kName,
            BrowserContextDependencyManager::GetInstance()) {}
  ~ArcBrowserContextKeyedServiceFactoryBase() override = default;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    auto* arc_service_manager = arc::ArcServiceManager::Get();

    // Practically, this is in testing case.
    if (!arc_service_manager) {
      VLOG(2) << "ArcServiceManager is not available.";
      return nullptr;
    }

    if (arc_service_manager->browser_context() != context) {
      VLOG(2) << "Non ARC allowed browser context.";
      return nullptr;
    }

    return new Service(context, arc_service_manager->arc_bridge_service());
  }
};

}  // namespace internal
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_BASE_H_
