// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/segmentation_platform/public/database_client.h"

class PrefService;

namespace segmentation_platform {
class SegmentationPlatformService;
}  // namespace segmentation_platform

namespace tips {

using ResultStatus = segmentation_platform::DatabaseClient::ResultStatus;

// Service to handle Tips notifications and orchestrate modular TipsFeature
// implementations.
class TipsService : public KeyedService, public base::SupportsUserData {
 public:
  using OnBestTipChosen =
      base::OnceCallback<void(std::optional<TipsNotificationsFeatureType>)>;

  TipsService(
      PrefService* pref_service,
      segmentation_platform::SegmentationPlatformService* segmentation_service);
  ~TipsService() override;

  TipsService(const TipsService&) = delete;
  TipsService& operator=(const TipsService&) = delete;

  // Registers a modular feature implementation with the service.
  void RegisterFeature(std::unique_ptr<TipsFeature> feature);

  // Queries database signals for all registered features via the segmentation
  // platform and determines the best eligible tip to display.
  virtual void DetermineBestTip(OnBestTipChosen callback);

 private:
  void OnFeaturesProcessed(
      OnBestTipChosen callback,
      ResultStatus status,
      const segmentation_platform::ModelProvider::Request& inputs);

  raw_ptr<PrefService> pref_service_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_service_;

  std::vector<std::unique_ptr<TipsFeature>> registered_features_;

  base::WeakPtrFactory<TipsService> weak_ptr_factory_{this};
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_
