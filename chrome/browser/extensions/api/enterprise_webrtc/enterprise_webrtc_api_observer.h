// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class EnterpriseWebrtcApiObserver : public BrowserContextKeyedAPI,
                                    public ExtensionRegistryObserver {
 public:
  explicit EnterpriseWebrtcApiObserver(content::BrowserContext* context);
  ~EnterpriseWebrtcApiObserver() override;

  EnterpriseWebrtcApiObserver(const EnterpriseWebrtcApiObserver&) = delete;
  EnterpriseWebrtcApiObserver& operator=(const EnterpriseWebrtcApiObserver&) =
      delete;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  static EnterpriseWebrtcApiObserver* Get(content::BrowserContext* context);

 private:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<EnterpriseWebrtcApiObserver>*
  GetFactoryInstance();
  static const char* service_name() { return "EnterpriseWebrtcApiObserver"; }
  static const bool kServiceIsCreatedWithBrowserContext = true;
  static const bool kServiceIsCreatedInGuestMode = false;
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceRedirectedInIncognito = false;

  friend class BrowserContextKeyedAPIFactory<EnterpriseWebrtcApiObserver>;

  const raw_ptr<content::BrowserContext> browser_context_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

class EnterpriseWebrtcApiObserverFactory
    : public BrowserContextKeyedAPIFactory<EnterpriseWebrtcApiObserver> {
 public:
  static EnterpriseWebrtcApiObserverFactory* GetInstance();

 private:
  friend base::NoDestructor<EnterpriseWebrtcApiObserverFactory>;

  EnterpriseWebrtcApiObserverFactory();
  ~EnterpriseWebrtcApiObserverFactory() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_OBSERVER_H_
