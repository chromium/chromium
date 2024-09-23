// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class PasswordsPrivateDelegate;

// Wrapper class around PasswordsPrivateDelegate to control its lifespan. If
// the new PasswordManagerUI is enabled callers have to hold scoped_refptr of
// PasswordsPrivateDelegate so the object can be released when no longer needed.
// If the feature is disabled this class always holds a
// scoped_refptr of PasswordsPrivateDelegate meaning it will live as long as
// BrowserContext lives.
class PasswordsPrivateDelegateProxy : public KeyedService {
 public:
  explicit PasswordsPrivateDelegateProxy(
      content::BrowserContext* browser_context);

  // Should be used only for testing.
  PasswordsPrivateDelegateProxy(
      content::BrowserContext* browser_context,
      scoped_refptr<PasswordsPrivateDelegate> delegate);
  ~PasswordsPrivateDelegateProxy() override;

  scoped_refptr<PasswordsPrivateDelegate> GetOrCreateDelegate();
  scoped_refptr<PasswordsPrivateDelegate> GetDelegate();

 private:
  // KeyedService
  void Shutdown() override;

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  base::WeakPtr<PasswordsPrivateDelegate> weak_instance_;
};

// Factory for creating PasswordPrivateDelegates.
// TODO(crbug.com/40255236): Replace with KeyedServiceFactory.
class PasswordsPrivateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static scoped_refptr<PasswordsPrivateDelegate> GetForBrowserContext(
      content::BrowserContext* browser_context,
      bool create);

  static PasswordsPrivateDelegateFactory* GetInstance();

 private:
  friend class base::NoDestructor<PasswordsPrivateDelegateFactory>;
  PasswordsPrivateDelegateFactory();
  ~PasswordsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_
