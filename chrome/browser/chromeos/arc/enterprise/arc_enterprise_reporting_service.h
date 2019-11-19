// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/enterprise_reporting.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

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

  ArcEnterpriseReportingService(content::BrowserContext* context,
                                ArcBridgeService* arc_bridge_service);
  ~ArcEnterpriseReportingService() override;

  // mojom::EnterpriseReportingHost overrides:
  void ReportManagementState(mojom::ManagementState state) override;

 private:
  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  base::WeakPtrFactory<ArcEnterpriseReportingService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcEnterpriseReportingService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
