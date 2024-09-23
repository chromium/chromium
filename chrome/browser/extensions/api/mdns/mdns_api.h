// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MDNS_MDNS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MDNS_MDNS_API_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {
// MDnsAPI is instantiated with the profile and will listen for extensions that
// register listeners for the chrome.mdns extension API. It will use a registry
// class to start the mDNS listener process (if necessary) and observe new
// service events to dispatch them to registered extensions.
class MDnsAPI : public BrowserContextKeyedAPI,
                public EventRouter::Observer,
                public media_router::DnsSdRegistry::DnsSdObserver {
 public:
  explicit MDnsAPI(content::BrowserContext* context);

  MDnsAPI(const MDnsAPI&) = delete;
  MDnsAPI& operator=(const MDnsAPI&) = delete;

  ~MDnsAPI() override;

  static MDnsAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MDnsAPI>* GetFactoryInstance();

  // Used to mock out the DnsSdRegistry for testing. Does not take ownership of
  // |registry|.
  void SetDnsSdRegistryForTesting(media_router::DnsSdRegistry* registry);

  // Immediately issues a multicast DNS query for all service types.
  // NOTE: Discovery queries are sent to all event handlers associated with
  // |this| service's BrowserContext.
  void ForceDiscovery();

 protected:
  // Retrieve an instance of the registry. Lazily created when needed.
  virtual media_router::DnsSdRegistry* dns_sd_registry();

  // Gets the list of mDNS event listeners.
  virtual const extensions::EventListenerMap::ListenerList& GetEventListeners();

 private:
  FRIEND_TEST_ALL_PREFIXES(MDnsAPIDiscoveryTest,
                           ServiceListenersAddedAndRemoved);

  typedef std::map<std::string, int> ServiceTypeCounts;
  friend class BrowserContextKeyedAPIFactory<MDnsAPI>;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // DnsSdRegistry::Observer
  void OnDnsSdEvent(
      const std::string& service_type,
      const media_router::DnsSdRegistry::DnsSdServiceList& services) override;
  void OnDnsSdPermissionRejected() override {}

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "MDnsAPI"; }

  static const bool kServiceIsCreatedWithBrowserContext = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Update the current list of service types and update the registry.
  void UpdateMDnsListeners();

  // Write a message to the consoles of extensions listening to a given service
  // type.
  void WriteToConsole(const std::string& service_type,
                      blink::mojom::ConsoleMessageLevel level,
                      const std::string& message);

  // Returns true if an extension or platform app |extension_id| is allowed to
  // listen to mDNS events.
  virtual bool IsMDnsAllowed(const std::string& extension_id) const;

  // Finds all all the valid listeners of the mdns.onServiceList event and
  // filters them by service type if |service_type_filter| is non-empty.
  // The list of extensions with active listeners is written to |extension_ids|,
  // if non-null.
  // The counts for each service type are output to |service_type_counts|, if
  // non-null.
  void GetValidOnServiceListListeners(const std::string& service_type_filter,
                                      std::set<std::string>* extension_ids,
                                      ServiceTypeCounts* service_type_counts);

  // Ensure methods are only called on UI thread.
  base::ThreadChecker thread_checker_;
  const raw_ptr<content::BrowserContext> browser_context_;
  // Raw pointer to a leaky singleton. Lazily created on first access. Must
  // outlive this object.
  raw_ptr<media_router::DnsSdRegistry> dns_sd_registry_;
  // Count of active listeners per service type, saved from the previous
  // invocation of UpdateMDnsListeners().
  ServiceTypeCounts prev_service_counts_;
};

class MdnsForceDiscoveryFunction : public ExtensionFunction {
 public:
  MdnsForceDiscoveryFunction() = default;

  MdnsForceDiscoveryFunction(const MdnsForceDiscoveryFunction&) = delete;
  MdnsForceDiscoveryFunction& operator=(const MdnsForceDiscoveryFunction&) =
      delete;

 protected:
  ~MdnsForceDiscoveryFunction() override = default;

 private:
  // ExtensionFunction override.
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("mdns.forceDiscovery", MDNS_FORCEDISCOVERY)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MDNS_MDNS_API_H_
