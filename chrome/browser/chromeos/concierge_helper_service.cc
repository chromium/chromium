// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/concierge_helper_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace {

void OnSetVmCpuRestriction(
    base::Optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response) {
  if (!response || !response->success()) {
    LOG(ERROR) << "Failed to call SetVmCpuRestriction";
    return;
  }
}

// Adds a callback to be run when Concierge DBus service becomes available.
// If the service is already available, runs the callback immediately.
void WaitForConciergeToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  auto* client = DBusThreadManager::Get()->GetConciergeClient();
  if (!client) {
    LOG(WARNING) << "ConciergeClient is not available";
    std::move(callback).Run(false);
    return;
  }
  client->WaitForServiceToBeAvailable(std::move(callback));
}

void SetVmCpuRestriction(
    vm_tools::concierge::SetVmCpuRestrictionRequest request,
    bool started) {
  if (!started) {
    LOG(ERROR) << "Unable to start Concierge to make a throttling request.";
    return;
  }

  auto* client = DBusThreadManager::Get()->GetConciergeClient();
  if (!client) {
    LOG(WARNING) << "ConciergeClient is not available";
    OnSetVmCpuRestriction(base::nullopt);
    return;
  }
  client->SetVmCpuRestriction(request, base::BindOnce(&OnSetVmCpuRestriction));
}

// Make a request to throttle or unthrottle a VM cgroup (according to
// do_restrict).
void MakeRestrictionRequest(vm_tools::concierge::CpuCgroup cgroup,
                            bool do_restrict) {
  vm_tools::concierge::SetVmCpuRestrictionRequest request;
  request.set_cpu_cgroup(cgroup);
  request.set_cpu_restriction_state(
      do_restrict ? vm_tools::concierge::CPU_RESTRICTION_BACKGROUND
                  : vm_tools::concierge::CPU_RESTRICTION_FOREGROUND);

  // Since ConciergeHelperService starts Concierge on construction,
  // and Concierge is auto-respawned by Upstart, the service must either be
  // available or restarting. We can run our throttle request as a callback to
  // WaitForConciergeToBeAvailable(): if Concierge is available, the request is
  // run immediately. Else, Concierge is restarting and the request will be run
  // when the service becomes available.
  WaitForConciergeToBeAvailable(
      base::BindOnce(&SetVmCpuRestriction, std::move(request)));
}

}  // namespace

// static
ConciergeHelperService* ConciergeHelperService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ConciergeHelperServiceFactory::GetForBrowserContext(context);
}

ConciergeHelperService::ConciergeHelperService() = default;

void ConciergeHelperService::SetArcVmCpuRestriction(bool do_restrict) {
  MakeRestrictionRequest(vm_tools::concierge::CPU_CGROUP_ARCVM, do_restrict);
}

void ConciergeHelperService::SetTerminaVmCpuRestriction(bool do_restrict) {
  MakeRestrictionRequest(vm_tools::concierge::CPU_CGROUP_TERMINA, do_restrict);
}

void ConciergeHelperService::SetPluginVmCpuRestriction(bool do_restrict) {
  MakeRestrictionRequest(vm_tools::concierge::CPU_CGROUP_PLUGINVM, do_restrict);
}

// static
ConciergeHelperServiceFactory* ConciergeHelperServiceFactory::GetInstance() {
  static base::NoDestructor<ConciergeHelperServiceFactory> instance;
  return instance.get();
}

// static
ConciergeHelperService* ConciergeHelperServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConciergeHelperService*>(
      ConciergeHelperServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, true /* create */));
}

ConciergeHelperServiceFactory::ConciergeHelperServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ConciergeHelperServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* ConciergeHelperServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;
  return new ConciergeHelperService();
}

}  // namespace chromeos
