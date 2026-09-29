// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_WEBRTC_ENTERPRISE_WEBRTC_API_OBSERVER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api::enterprise_webrtc {
struct PeerConnectionRecord;
}  // namespace api::enterprise_webrtc

class EnterpriseWebrtcApiObserver
    : public BrowserContextKeyedAPI,
      public ExtensionRegistryObserver,
      public content::WebRtcDiagnostics::Observer {
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

  // content::WebRtcDiagnostics::Observer implementation.
  void OnPeerConnectionAdded(
      const std::string& id,
      const base::Value& data,
      const std::vector<std::string>& matched_client_ids) override;
  void OnPeerConnectionRemoved(
      const std::string& id,
      const std::vector<std::string>& matched_client_ids) override;
  void OnCaptureStopped(const std::string& stopped_client_id) override;

  static EnterpriseWebrtcApiObserver* Get(content::BrowserContext* context);

 private:
  // Dispatches onPeerConnectionAdded to `extension_id`.
  void DispatchPeerConnectionAdded(
      const std::string& id,
      const api::enterprise_webrtc::PeerConnectionRecord& data,
      const std::string& extension_id);
  // Dispatches onPeerConnectionRemoved to `extension_id`.
  void DispatchPeerConnectionRemoved(const std::string& id,
                                     const std::string& extension_id);
  // Dispatches onCaptureStopped to `extension_id`.
  void DispatchCaptureStopped(const std::string& extension_id);

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
