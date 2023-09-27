// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_CHROME_FEATURE_FLAGS_BRIDGE_H_
#define ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_CHROME_FEATURE_FLAGS_BRIDGE_H_

#include "ash/components/arc/mojom/chrome_feature_flags.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome feature flag states to the ARC.
class ArcChromeFeatureFlagsBridge
    : public KeyedService,
      public ConnectionObserver<mojom::ChromeFeatureFlagsInstance>,
      public mojom::ChromeFeatureFlagsHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcChromeFeatureFlagsBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcChromeFeatureFlagsBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcChromeFeatureFlagsBridge(content::BrowserContext* context,
                              ArcBridgeService* bridge_service);

  ArcChromeFeatureFlagsBridge(const ArcChromeFeatureFlagsBridge&) = delete;
  ArcChromeFeatureFlagsBridge& operator=(const ArcChromeFeatureFlagsBridge&) =
      delete;

  ~ArcChromeFeatureFlagsBridge() override;

  // ConnectionObserver<mojom::ChromeFeatureFlagsInstance> overrides:
  void OnConnectionReady() override;

  static void EnsureFactoryBuilt();

  // mojom::ChromeFeatureFlagHost overrides:
  // Get feature flag enabled / disabled state by feature name. If the feature
  // is not ARC related feature, it will return `null_opt`.
  void IsFeatureEnabled(const std::string& feature_name,
                        IsFeatureEnabledCallback callback) override;

  // Get int feature parameters by feature name and parameter name. If the
  // feature parameter doesn't exist, it will return `null_opt`.
  void GetIntParamByFeatureAndParamName(
      const std::string& feature_name,
      const std::string& param_name,
      GetIntParamByFeatureAndParamNameCallback callback) override;

  // Get dobule feature parameters by feature name and parameter name. If the
  // feature parameter doesn't exist, it will return `null_opt`.
  void GetDoubleParamByFeatureAndParamName(
      const std::string& feature_name,
      const std::string& param_name,
      GetDoubleParamByFeatureAndParamNameCallback callback) override;

  // Get bool feature parameters by feature name and parameter name. If the
  // feature parameter doesn't exist, it will return `null_opt`.
  void GetBoolParamByFeatureAndParamName(
      const std::string& feature_name,
      const std::string& param_name,
      GetBoolParamByFeatureAndParamNameCallback callback) override;

 private:
  THREAD_CHECKER(thread_checker_);

  void NotifyFeatureFlags();

  const raw_ptr<ArcBridgeService, ExperimentalAsh>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_CHROME_FEATURE_FLAGS_BRIDGE_H_
