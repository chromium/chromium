// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

using content::BrowserContext;

PasswordsPrivateDelegateProxy::PasswordsPrivateDelegateProxy(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

PasswordsPrivateDelegateProxy::PasswordsPrivateDelegateProxy(
    BrowserContext* browser_context,
    scoped_refptr<PasswordsPrivateDelegate> delegate)
    : browser_context_(browser_context) {
  weak_instance_ = delegate->AsWeakPtr();
}
PasswordsPrivateDelegateProxy::~PasswordsPrivateDelegateProxy() = default;

void PasswordsPrivateDelegateProxy::Shutdown() {
  browser_context_ = nullptr;
  weak_instance_ = nullptr;
}

scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateProxy::GetOrCreateDelegate() {
  if (weak_instance_) {
    return scoped_refptr<PasswordsPrivateDelegate>(weak_instance_.get());
  }

  scoped_refptr<PasswordsPrivateDelegate> manager =
      base::MakeRefCounted<PasswordsPrivateDelegateImpl>(
          static_cast<Profile*>(browser_context_));
  weak_instance_ = manager->AsWeakPtr();
  return manager;
}

scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateProxy::GetDelegate() {
  return scoped_refptr<PasswordsPrivateDelegate>(weak_instance_.get());
}

// static
scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context,
    bool create) {
  PasswordsPrivateDelegateProxy* proxy =
      static_cast<PasswordsPrivateDelegateProxy*>(
          GetInstance()->GetServiceForBrowserContext(browser_context, true));
  return create ? proxy->GetOrCreateDelegate() : proxy->GetDelegate();
}

// static
PasswordsPrivateDelegateFactory*
    PasswordsPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<PasswordsPrivateDelegateFactory> instance;
  return instance.get();
}

PasswordsPrivateDelegateFactory::PasswordsPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "PasswordsPrivateDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(BulkLeakCheckServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(PasswordsPrivateEventRouterFactory::GetInstance());
}

PasswordsPrivateDelegateFactory::~PasswordsPrivateDelegateFactory() = default;

std::unique_ptr<KeyedService>
PasswordsPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<PasswordsPrivateDelegateProxy>(profile);
}

}  // namespace extensions
