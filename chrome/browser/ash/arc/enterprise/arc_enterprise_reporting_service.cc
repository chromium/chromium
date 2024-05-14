// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_enterprise_reporting_service.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

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

const char* TimedCloudDpcOpToString(mojom::TimedCloudDpcOp op) {
  switch (op) {
    case mojom::TimedCloudDpcOp::UNKNOWN_OP:
      NOTREACHED_IN_MIGRATION();  // handled by if-statement in calling method
      return "";
    case mojom::TimedCloudDpcOp::SETUP_TOTAL:
      return "SetupService.Total";
    case mojom::TimedCloudDpcOp::SETUP_PULL_AND_APPLY_POLICIES:
      return "SetupService.PullAndApplyPolicies";
    case mojom::TimedCloudDpcOp::SETUP_ADD_ACCOUNT:
      return "SetupService.AddAccount";
    case mojom::TimedCloudDpcOp::SETUP_INSTALL_APPS:
      return "SetupService.InstallApps";
    case mojom::TimedCloudDpcOp::DEVICE_SETUP:
      return "DeviceSetup";
    case mojom::TimedCloudDpcOp::SETUP_CHECK_FOR_ANDROID_ID:
    case mojom::TimedCloudDpcOp::SETUP_CHECK_FOR_FIRST_ACCOUNT_READY:
    case mojom::TimedCloudDpcOp::SETUP_REGISTER:
    case mojom::TimedCloudDpcOp::SETUP_REPORT_POLICY_COMPLIANCE:
    case mojom::TimedCloudDpcOp::SETUP_QUARANTINED:
    case mojom::TimedCloudDpcOp::SETUP_INSTALL_APPS_RETRY:
    case mojom::TimedCloudDpcOp::SETUP_UPDATE_PLAY_SERVICES:
    case mojom::TimedCloudDpcOp::SETUP_CHECK_REGISTRATION_TOKEN:
    case mojom::TimedCloudDpcOp::SETUP_THIRD_PARTY_SIGNIN:
      return ""; // Deprecated and unused
  }

  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace

// static
ArcEnterpriseReportingService*
ArcEnterpriseReportingService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcEnterpriseReportingServiceFactory::GetForBrowserContext(context);
}

// static
ArcEnterpriseReportingService*
ArcEnterpriseReportingService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcEnterpriseReportingServiceFactory::GetForBrowserContextForTesting(
      context);
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

void ArcEnterpriseReportingService::ReportCloudDpcOperationTime(
    int64_t time_ms,
    mojom::TimedCloudDpcOp op,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (op != mojom::TimedCloudDpcOp::UNKNOWN_OP) {
    const std::string histogram_name =
        base::StrCat({"Arc.CloudDpc.", TimedCloudDpcOpToString(op),
                      ".TimeDelta", success ? ".Success" : ".Failure"});

    base::UmaHistogramMediumTimes(
        GetHistogramNameByUserTypeForPrimaryProfile(histogram_name),
        base::Milliseconds(time_ms));
  } else {
    DLOG(ERROR) << "Attempted to record time for unknown op";
  }
}

// static
void ArcEnterpriseReportingService::EnsureFactoryBuilt() {
  ArcEnterpriseReportingServiceFactory::GetInstance();
}

}  // namespace arc
