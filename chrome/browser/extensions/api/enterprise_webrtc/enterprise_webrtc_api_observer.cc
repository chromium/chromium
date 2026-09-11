// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

EnterpriseWebrtcApiObserver::EnterpriseWebrtcApiObserver(
    content::BrowserContext* context)
    : browser_context_(context) {
  // ExtensionRegistry is shared between a profile and its incognito profile,
  // while this class has an instance in each. Both instances therefore see
  // every unload, and each tears down the session in its own profile, which
  // is what an uninstall needs.
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));

  // WebRtcDiagnostics is a never-destroyed singleton, so it outlives this
  // observer and every BrowserContext; registering here and removing in the
  // destructor is always safe.
  content::WebRtcDiagnostics::GetInstance()->AddObserver(browser_context_,
                                                         this);
}

EnterpriseWebrtcApiObserver::~EnterpriseWebrtcApiObserver() {
  content::WebRtcDiagnostics::GetInstance()->RemoveObserver(browser_context_,
                                                            this);
}

// static
EnterpriseWebrtcApiObserver* EnterpriseWebrtcApiObserver::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<EnterpriseWebrtcApiObserver>::Get(
      context);
}

// static
BrowserContextKeyedAPIFactory<EnterpriseWebrtcApiObserver>*
EnterpriseWebrtcApiObserver::GetFactoryInstance() {
  return EnterpriseWebrtcApiObserverFactory::GetInstance();
}

void EnterpriseWebrtcApiObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Deliberately uses `browser_context_` rather than the `browser_context` this
  // was called with: each instance is responsible for its own profile. The
  // instance belonging to the other profile receives this same notification
  // and stops its own session.
  content::WebRtcDiagnostics::GetInstance()->StopCaptureForClient(
      browser_context_, extension->id());
}

void EnterpriseWebrtcApiObserver::OnCaptureStopped(
    std::string_view stopped_client_id) {
  DispatchCaptureStopped(std::string(stopped_client_id));
}

void EnterpriseWebrtcApiObserver::DispatchCaptureStopped(
    const std::string& extension_id) {
  auto event = std::make_unique<Event>(
      events::ENTERPRISE_WEBRTC_ON_CAPTURE_STOPPED,
      api::enterprise_webrtc::OnCaptureStopped::kEventName,
      api::enterprise_webrtc::OnCaptureStopped::Create(), browser_context_);
  // The extension may no longer be enabled, e.g. when the capture stopped
  // because it was unloaded. EventRouter drops the event in that case.
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

// static
EnterpriseWebrtcApiObserverFactory*
EnterpriseWebrtcApiObserverFactory::GetInstance() {
  static base::NoDestructor<EnterpriseWebrtcApiObserverFactory> instance;
  return instance.get();
}

EnterpriseWebrtcApiObserverFactory::EnterpriseWebrtcApiObserverFactory() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
}

EnterpriseWebrtcApiObserverFactory::~EnterpriseWebrtcApiObserverFactory() =
    default;

}  // namespace extensions
