// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/mdns_api.h"

#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

namespace mdns = api::mdns;

namespace {

// Whitelisted mDNS service types.
const char kCastServiceType[] = "_googlecast._tcp.local";
const char kPrivetServiceType[] = "_privet._tcp.local";
const char kTestServiceType[] = "_testing._tcp.local";

bool IsServiceTypeWhitelisted(const std::string& service_type) {
  return service_type == kCastServiceType ||
         service_type == kPrivetServiceType ||
         service_type == kTestServiceType;
}

}  // namespace

using DnsSdRegistry = media_router::DnsSdRegistry;

MDnsAPI::MDnsAPI(content::BrowserContext* context)
    : browser_context_(context), dns_sd_registry_(nullptr) {
  DCHECK(browser_context_);
  extensions::EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  event_router->RegisterObserver(this, mdns::OnServiceList::kEventName);
}

MDnsAPI::~MDnsAPI() {
  if (dns_sd_registry_) {
    dns_sd_registry_->RemoveObserver(this);
  }
}

// static
MDnsAPI* MDnsAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MDnsAPI>::Get(context);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<MDnsAPI>>::
    DestructorAtExit g_mdns_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MDnsAPI>* MDnsAPI::GetFactoryInstance() {
  return g_mdns_api_factory.Pointer();
}

void MDnsAPI::SetDnsSdRegistryForTesting(DnsSdRegistry* dns_sd_registry) {
  dns_sd_registry_ = dns_sd_registry;
  if (dns_sd_registry_)
    dns_sd_registry_->AddObserver(this);
}

void MDnsAPI::ForceDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DnsSdRegistry* registry = dns_sd_registry();
  return registry->ResetAndDiscover();
}

DnsSdRegistry* MDnsAPI::dns_sd_registry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dns_sd_registry_) {
    dns_sd_registry_ = media_router::DnsSdRegistry::GetInstance();
    dns_sd_registry_->AddObserver(this);
  }
  return dns_sd_registry_;
}

void MDnsAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateMDnsListeners();
}

void MDnsAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateMDnsListeners();
}

void MDnsAPI::UpdateMDnsListeners() {
  std::set<std::string> new_service_types;
  ServiceTypeCounts current_service_counts;
  GetValidOnServiceListListeners(
      "" /* service_type_filter - blank = all services */,
      nullptr /* extension_ids */, &current_service_counts);

  DnsSdRegistry* registry = dns_sd_registry();

  // Check if the counts of per-service-type event handlers has changed since
  // the previous invocation, and take appropriate action if a change was
  // detected.
  //
  // mDNS registration is performed for difference(cur, previous).
  // mDNS unregistration is performed for difference(previous, cur).
  // The mDNS device list is refreshed if the listener count has grown for
  // a service type in union(cur, previous).
  auto i_cur = current_service_counts.begin();
  auto i_prev = prev_service_counts_.begin();
  while (i_cur != current_service_counts.end() ||
         i_prev != prev_service_counts_.end()) {
    if (i_prev == prev_service_counts_.end() ||
        (i_cur != current_service_counts.end() &&
         i_cur->first < i_prev->first)) {
      DVLOG(2) << "Registering listener for mDNS service " << i_cur->first;
      registry->RegisterDnsSdListener(i_cur->first);
      i_cur++;
    } else if (i_cur == current_service_counts.end() ||
               (i_prev != prev_service_counts_.end() &&
                i_prev->first < i_cur->first)) {
      DVLOG(2) << "Unregistering listener for mDNS service " << i_prev->first;
      registry->UnregisterDnsSdListener(i_prev->first);
      i_prev++;
    } else {
      if (i_cur->second > i_prev->second) {
        DVLOG(2) << "Additional listeners added for mDNS service "
                 << i_cur->first;
        registry->Publish(i_cur->first);
      }
      ++i_cur;
      ++i_prev;
    }
  }
  prev_service_counts_.swap(current_service_counts);
}

void MDnsAPI::OnDnsSdEvent(const std::string& service_type,
                           const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<mdns::MDnsService> args;
  for (const auto& service : services) {
    if (static_cast<int>(args.size()) ==
        api::mdns::MAX_SERVICE_INSTANCES_PER_EVENT) {
      // TODO(reddaly): This is not the most meaningful way of notifying the
      // application that something bad happened.  It will go to the user's
      // console (which most users don't look at)and the developer will be none
      // the wiser.  Instead, changing the event to pass the number of
      // discovered instances would allow the caller to know when the list is
      // truncated and tell the user something meaningful in the extension/app.
      WriteToConsole(
          service_type, blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf("Truncating number of service instances in "
                             "onServiceList to maximum allowed: %d",
                             api::mdns::MAX_SERVICE_INSTANCES_PER_EVENT));
      break;
    }
    mdns::MDnsService mdns_service;
    mdns_service.service_name = service.service_name;
    mdns_service.service_host_port = service.service_host_port.ToString();
    mdns_service.ip_address = service.ip_address;
    mdns_service.service_data = service.service_data;
    args.push_back(std::move(mdns_service));
  }

  std::unique_ptr<base::ListValue> results = mdns::OnServiceList::Create(args);
  auto event = std::make_unique<Event>(events::MDNS_ON_SERVICE_LIST,
                                       mdns::OnServiceList::kEventName,
                                       std::move(results), browser_context_);
  event->filter_info.service_type = service_type;

  // TODO(justinlin): To avoid having listeners without filters getting all
  // events, modify API to have this event require filters.
  // TODO(reddaly): If event isn't on whitelist, ensure it does not get
  // broadcast to extensions.
  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

const extensions::EventListenerMap::ListenerList& MDnsAPI::GetEventListeners() {
  return extensions::EventRouter::Get(browser_context_)
      ->listeners()
      .GetEventListenersByName(mdns::OnServiceList::kEventName);
}

bool MDnsAPI::IsMDnsAllowed(const std::string& extension_id,
                            const std::string& service_type) const {
  const extensions::Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->enabled_extensions()
          .GetByID(extension_id);
  return (extension && (extension->is_platform_app() ||
                        IsServiceTypeWhitelisted(service_type)));
}

void MDnsAPI::GetValidOnServiceListListeners(
    const std::string& service_type_filter,
    std::set<std::string>* extension_ids,
    ServiceTypeCounts* service_type_counts) {
  for (const auto& listener : GetEventListeners()) {
    base::DictionaryValue* filter = listener->filter();

    std::string service_type;
    filter->GetStringASCII(kEventFilterServiceTypeKey, &service_type);
    if (service_type.empty())
      continue;

    // Match service type when filter isn't ""
    if (!service_type_filter.empty() && service_type_filter != service_type)
      continue;

    // Don't listen for services associated only with disabled extensions
    // or non-whitelisted, non-platform-app extensions.
    if (!IsMDnsAllowed(listener->extension_id(), service_type))
      continue;

    if (extension_ids)
      extension_ids->insert(listener->extension_id());
    if (service_type_counts) {
      (*service_type_counts)[service_type]++;
    }
  }
}

void MDnsAPI::WriteToConsole(const std::string& service_type,
                             blink::mojom::ConsoleMessageLevel level,
                             const std::string& message) {
  // Get all the extensions with an onServiceList listener for a particular
  // service type.
  std::set<std::string> extension_ids;
  ServiceTypeCounts counts;
  GetValidOnServiceListListeners(service_type, &extension_ids,
                                 nullptr /* service_type_counts */);

  std::string logged_message(std::string("[chrome.mdns] ") + message);

  // Log to the consoles of the background pages for those extensions.
  // TODO(devlin): It's a little weird to log to the background pages,
  // especially when it might be dormant. We should probably just log to a place
  // like the ErrorConsole instead.
  for (const std::string& extension_id : extension_ids) {
    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(browser_context_)
        ->GetBackgroundHostForExtension(extension_id);
    content::RenderFrameHost* rfh =
        host ? host->host_contents()->GetMainFrame() : nullptr;
    if (rfh)
      rfh->AddMessageToConsole(level, logged_message);
  }
}

MdnsForceDiscoveryFunction::MdnsForceDiscoveryFunction() {
}

MdnsForceDiscoveryFunction::~MdnsForceDiscoveryFunction() {
}

AsyncApiFunction::ResponseAction MdnsForceDiscoveryFunction::Run() {
  MDnsAPI* api = MDnsAPI::Get(browser_context());
  if (!api) {
    return RespondNow(Error("Unknown error."));
  }
  api->ForceDiscovery();
  return RespondNow(NoArguments());
}

}  // namespace extensions
