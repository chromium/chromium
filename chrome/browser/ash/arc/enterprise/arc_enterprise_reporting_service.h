// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_

#include "ash/components/arc/mojom/enterprise_reporting.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class controls the ARC enterprise reporting.
class ArcEnterpriseReportingService
    : public KeyedService,
      public mojom::EnterpriseReportingHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcEnterpriseReportingService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcEnterpriseReportingService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcEnterpriseReportingService(content::BrowserContext* context,
                                ArcBridgeService* arc_bridge_service);

  ArcEnterpriseReportingService(const ArcEnterpriseReportingService&) = delete;
  ArcEnterpriseReportingService& operator=(
      const ArcEnterpriseReportingService&) = delete;

  ~ArcEnterpriseReportingService() override;

  // mojom::EnterpriseReportingHost overrides:
  void ReportCloudDpcOperationTime(int64_t time_ms,
                                   mojom::TimedCloudDpcOp op,
                                   bool success) override;

  static void EnsureFactoryBuilt();

 private:
  THREAD_CHECKER(thread_checker_);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  base::WeakPtrFactory<ArcEnterpriseReportingService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
