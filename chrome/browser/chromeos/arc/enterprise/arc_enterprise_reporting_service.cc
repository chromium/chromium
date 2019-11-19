// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/arc_enterprise_reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {
namespace {

// Singleton factory for ArcEnterpriseReportingService.
class ArcEnterpriseReportingServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcEnterpriseReportingService,
          ArcEnterpriseReportingServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcEnterpriseReportingServiceFactory";

  static ArcEnterpriseReportingServiceFactory* GetInstance() {
    return base::Singleton<ArcEnterpriseReportingServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcEnterpriseReportingServiceFactory>;
  ArcEnterpriseReportingServiceFactory() = default;
  ~ArcEnterpriseReportingServiceFactory() override = default;
};

}  // namespace

// static
ArcEnterpriseReportingService*
ArcEnterpriseReportingService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcEnterpriseReportingServiceFactory::GetForBrowserContext(context);
}

ArcEnterpriseReportingService::ArcEnterpriseReportingService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->enterprise_reporting()->SetHost(this);
}

ArcEnterpriseReportingService::~ArcEnterpriseReportingService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->enterprise_reporting()->SetHost(nullptr);
}

void ArcEnterpriseReportingService::ReportManagementState(
    mojom::ManagementState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "ReportManagementState state=" << state;

  if (state == mojom::ManagementState::MANAGED_DO_LOST) {
    DCHECK(ArcServiceManager::Get());
    VLOG(1) << "Management state lost. Removing ARC user data.";
    ArcSessionManager::Get()->RequestArcDataRemoval();
    ArcSessionManager::Get()->StopAndEnableArc();
  }
}

}  // namespace arc
