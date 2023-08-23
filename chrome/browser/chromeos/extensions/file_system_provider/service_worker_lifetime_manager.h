// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_SERVICE_WORKER_LIFETIME_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_SERVICE_WORKER_LIFETIME_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ProcessManager;
struct EventTarget;

namespace file_system_provider {

// Identifies a unique fileSystemProvider request: request ID sequence of
// integers tracked per a filesystem instance, or per provider (extension) for
// requests that aren't specific to a filesystem instance.
struct RequestKey {
  extensions::ExtensionId extension_id;
  std::string file_system_id;
  int64_t request_id;

  bool operator<(const RequestKey& other) const;
};

// Tracks fileSystemProvider requests that have been dispatched to service
// workers but not replied to yet, and keeps service workers alive while there
// are requests in progress.
class ServiceWorkerLifetimeManager : public KeyedService {
 public:
  explicit ServiceWorkerLifetimeManager(content::BrowserContext*);
  ServiceWorkerLifetimeManager(const ServiceWorkerLifetimeManager&) = delete;
  ServiceWorkerLifetimeManager& operator=(const ServiceWorkerLifetimeManager&) =
      delete;
  ~ServiceWorkerLifetimeManager() override;

  static ServiceWorkerLifetimeManager* Get(content::BrowserContext*);

  // Signals that a request has been sent to a fileSystemProvider. Called when
  // the request is about to be dispatched (the actual targets that received the
  // request aren't known yet).
  void StartRequest(const RequestKey&);
  // Signals that a request previously sent to a fileSystemProvider has
  // finished. Called either when a request has been replied to (the first
  // response finishes the request), or is cancelled, due to timeout or being
  // aborted.
  void FinishRequest(const RequestKey&);
  // Signals that a request has been dispatched to a service worker with
  // registered fileSystemProvider listeners. Called for each service worker the
  // request has been dispatched to.
  void RequestDispatched(const RequestKey&, const EventTarget&);
  // KeyedService:
  void Shutdown() override;

  // Helper to create a callback for when an event is dispatched. The callback
  // is safe as it handles this object's lifetime.
  Event::DidDispatchCallback CreateDispatchCallbackForRequest(
      const RequestKey&);

 protected:
  struct KeepaliveKey {
    WorkerId worker_id;
    std::string request_uuid;

    bool operator==(const KeepaliveKey& other) const;
    bool operator<(const KeepaliveKey& other) const;
  };

  // Virtual for tests.
  virtual std::string IncrementKeepalive(const WorkerId&);
  virtual void DecrementKeepalive(const KeepaliveKey&);

 private:
  friend class ServiceWorkerLifetimeManagerFactory;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerLifetimeManagerTest,
                           TestDispatchMultipleEvents);

  raw_ptr<ProcessManager> process_manager_;
  std::map<RequestKey, std::set<KeepaliveKey>> requests_;

  base::WeakPtrFactory<ServiceWorkerLifetimeManager> weak_ptr_factory_{this};
};

// KeyedService factory for ServiceWorkerLifetimeManager.
class ServiceWorkerLifetimeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ServiceWorkerLifetimeManagerFactory(
      const ServiceWorkerLifetimeManagerFactory&) = delete;
  ServiceWorkerLifetimeManagerFactory& operator=(
      const ServiceWorkerLifetimeManagerFactory&) = delete;

  static ServiceWorkerLifetimeManager* GetForBrowserContext(
      content::BrowserContext*);
  static ServiceWorkerLifetimeManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ServiceWorkerLifetimeManagerFactory>;

  ServiceWorkerLifetimeManagerFactory();
  ~ServiceWorkerLifetimeManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace file_system_provider
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_SERVICE_WORKER_LIFETIME_MANAGER_H_
