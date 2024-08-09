// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/concierge_helper/concierge_helper_service.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace {

void OnSetVmCpuRestriction(
    std::optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response) {
  if (!response || !response->success()) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Failed to call SetVmCpuRestriction";
    return;
  }
}

// Adds a callback to be run when Concierge DBus service becomes available.
// If the service is already available, runs the callback immediately.
void WaitForConciergeToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  auto* client = ConciergeClient::Get();
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

  auto* client = ConciergeClient::Get();
  if (!client) {
    LOG(WARNING) << "ConciergeClient is not available";
    OnSetVmCpuRestriction(std::nullopt);
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
    : ProfileKeyedServiceFactory(
          "ConciergeHelperServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
ConciergeHelperServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ConciergeHelperService>();
}

}  // namespace ash
