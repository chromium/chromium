// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/users_private/users_private_delegate_factory.h"

#include "chrome/browser/ash/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

using content::BrowserContext;

// static
UsersPrivateDelegate* UsersPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<UsersPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
UsersPrivateDelegateFactory* UsersPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<UsersPrivateDelegateFactory> instance;
  return instance.get();
}

UsersPrivateDelegateFactory::UsersPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "UsersPrivateDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

UsersPrivateDelegateFactory::~UsersPrivateDelegateFactory() = default;

std::unique_ptr<KeyedService>
UsersPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<UsersPrivateDelegate>(static_cast<Profile*>(profile));
}

}  // namespace extensions
