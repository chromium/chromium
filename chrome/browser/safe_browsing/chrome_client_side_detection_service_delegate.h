// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

// Delegate class which implements chrome specific bits for configuring
// the ClientSideDetectionService class.
class ChromeClientSideDetectionServiceDelegate
    : public ClientSideDetectionService::Delegate,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  explicit ChromeClientSideDetectionServiceDelegate(Profile* profile);

  ChromeClientSideDetectionServiceDelegate(
      const ChromeClientSideDetectionServiceDelegate&) = delete;
  ChromeClientSideDetectionServiceDelegate& operator=(
      const ChromeClientSideDetectionServiceDelegate&) = delete;

  ~ChromeClientSideDetectionServiceDelegate() override;

  // ClientSideDetectionService::Delegate implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSafeBrowsingURLLoaderFactory() override;
  bool ShouldSendModelToBrowserContext(
      content::BrowserContext* context) override;
  void StartListeningToOnDeviceModelUpdate() override;
  void StopListeningToOnDeviceModelUpdate() override;
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
  GetModelExecutorSession() override;
  void LogOnDeviceModelEligibilityReason() override;

 private:
  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  void NotifyServiceOnDeviceModelAvailable();

  // It is set to true when the on-device model is not readily available, but
  // it's expected to be ready soon. See `kWaitableReasons` for more details.
  bool observing_on_device_model_availability_ = false;

  base::TimeTicks on_device_fetch_time_;
  raw_ptr<Profile> profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
