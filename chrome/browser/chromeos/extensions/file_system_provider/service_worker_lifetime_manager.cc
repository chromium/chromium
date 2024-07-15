// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"

#include <tuple>
#include <utility>

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"

namespace extensions::file_system_provider {

bool RequestKey::operator<(const RequestKey& other) const {
  return std::tie(extension_id, file_system_id, request_id) <
         std::tie(other.extension_id, other.file_system_id, other.request_id);
}

ServiceWorkerLifetimeManager::ServiceWorkerLifetimeManager(
    content::BrowserContext* context)
    // Context can be null in tests.
    : process_manager_(context ? extensions::ProcessManager::Get(context)
                               : nullptr) {}

ServiceWorkerLifetimeManager::~ServiceWorkerLifetimeManager() = default;

ServiceWorkerLifetimeManager* ServiceWorkerLifetimeManager::Get(
    content::BrowserContext* context) {
  return ServiceWorkerLifetimeManagerFactory::GetForBrowserContext(context);
}

void ServiceWorkerLifetimeManager::StartRequest(const RequestKey& key) {
  DCHECK(!base::Contains(requests_, key));
  requests_[key] = {};
}

void ServiceWorkerLifetimeManager::FinishRequest(
    const RequestKey& request_key) {
  auto it = requests_.find(request_key);
  if (it == requests_.end()) {
    return;
  }
  std::set<KeepaliveKey> keepalive_keys = std::move(it->second);
  requests_.erase(it);
  for (const KeepaliveKey& keepalive_key : keepalive_keys) {
    DecrementKeepalive(keepalive_key);
  }
}

void ServiceWorkerLifetimeManager::RequestDispatched(
    const RequestKey& key,
    const EventTarget& target) {
  if (target.service_worker_version_id ==
      blink::mojom::kInvalidServiceWorkerVersionId) {
    return;
  }
  auto it = requests_.find(key);
  if (it == requests_.end()) {
    return;
  }
  std::set<KeepaliveKey>& keepalive_keys = it->second;
  WorkerId worker_id{
      target.extension_id,
      target.render_process_id,
      target.service_worker_version_id,
      target.worker_thread_id,
  };
  std::string uuid = IncrementKeepalive(worker_id);
  keepalive_keys.insert(KeepaliveKey{worker_id, uuid});
}

void ServiceWorkerLifetimeManager::Shutdown() {
  for (const auto& [_, keys] : requests_) {
    for (const KeepaliveKey& key : keys) {
      DecrementKeepalive(key);
    }
  }
}

Event::DidDispatchCallback
ServiceWorkerLifetimeManager::CreateDispatchCallbackForRequest(
    const RequestKey& request_key) {
  return base::BindRepeating(
      &extensions::file_system_provider::ServiceWorkerLifetimeManager::
          RequestDispatched,
      weak_ptr_factory_.GetWeakPtr(), request_key);
}

bool ServiceWorkerLifetimeManager::KeepaliveKey::operator<(
    const KeepaliveKey& other) const {
  return std::tie(worker_id, request_uuid) <
         std::tie(other.worker_id, other.request_uuid);
}

std::string ServiceWorkerLifetimeManager::IncrementKeepalive(
    const WorkerId& worker_id) {
  return process_manager_
      ->IncrementServiceWorkerKeepaliveCount(
          worker_id,
          content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
          extensions::Activity::Type::EVENT, /*extra_data=*/"")
      .AsLowercaseString();
}

void ServiceWorkerLifetimeManager::DecrementKeepalive(const KeepaliveKey& key) {
  base::Uuid uuid = base::Uuid::ParseLowercase(key.request_uuid);
  process_manager_->DecrementServiceWorkerKeepaliveCount(
      key.worker_id, uuid, extensions::Activity::Type::EVENT,
      /*extra_data=*/"");
}

// static
ServiceWorkerLifetimeManager*
ServiceWorkerLifetimeManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ServiceWorkerLifetimeManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ServiceWorkerLifetimeManagerFactory*
ServiceWorkerLifetimeManagerFactory::GetInstance() {
  return base::Singleton<ServiceWorkerLifetimeManagerFactory>::get();
}

ServiceWorkerLifetimeManagerFactory::ServiceWorkerLifetimeManagerFactory()
    : ProfileKeyedServiceFactory(
          "ServiceWorkerLifetimeManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::ProcessManagerFactory::GetInstance());
}

ServiceWorkerLifetimeManagerFactory::~ServiceWorkerLifetimeManagerFactory() =
    default;

std::unique_ptr<KeyedService>
ServiceWorkerLifetimeManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ServiceWorkerLifetimeManager>(context);
}

}  // namespace extensions::file_system_provider
