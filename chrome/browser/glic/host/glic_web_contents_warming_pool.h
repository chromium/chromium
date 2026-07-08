// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_

#include <memory>
#include <optional>

#include "base/feature.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class Profile;
namespace content {
class WebContents;
}

namespace glic {

BASE_DECLARE_FEATURE(kGlicReloadWebContentsAfterExpiry);

class WebUIContentsContainer;

// A pool for pre-warming Glic WebContents.
// This is used to reduce the perceived latency when opening the Glic UI by
// creating a WebContents in the background before it's actually needed.
class GlicWebContentsWarmingPool {
 public:
  enum class ClearReason {
    kShutdown,
    kMemoryPressure,
  };

  // LINT.IfChange(GlicContainerCreationReason)
  enum class ContainerCreationReason {
    kInitialColdWarming = 0,      // Preloaded after cold start.
    kUserTriggeredColdStart = 1,  // Created immediately during TakeContainer()
                                  // because the pool was empty
    kRefill = 2,  // Created to refill the pool after TakeContainer()
    kReloadAfterExpiry =
        3,  // Created to reload the pool after the previous container expired
    kMaxValue = kReloadAfterExpiry,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicContainerCreationReason)

  explicit GlicWebContentsWarmingPool(Profile* profile);
  virtual ~GlicWebContentsWarmingPool();

  // Retrieves a warmed WebUIContentsContainer from the pool. If no warmed
  // container is available, one will be created and then returned. A new
  // container is then preloaded in the background to replace the taken one.
  std::unique_ptr<WebUIContentsContainer> TakeContainer();
  // Checks resource constraints (e.g., memory pressure) and initiates
  // initial cold-start pre-warming if allowed. Returns true if pre-warming
  // proceeded, or false otherwise.
  bool MaybeStartInitialWarming();
  // Unconditionally ensures that a WebUIContentsContainer is preloaded. If the
  // existing one is crashed, it will be replaced.
  void EnsurePreload(ContainerCreationReason reason =
                         ContainerCreationReason::kUserTriggeredColdStart);
  // Clears the warming pool and destroys any warmed WebContents.
  void Clear(std::optional<ClearReason> reason);

  // Handles memory pressure notifications by clearing or statefully disabling
  // pre-warming, depending on feature configuration.
  void OnMemoryPressure(base::MemoryPressureLevel level);

  // LINT.IfChange(GlicWarmingPoolStatus)
  enum class WarmingPoolStatus {
    kHit = 0,
    kCold = 1,
    kExpired = 2,
    kCrashed = 3,
    kMemoryPressure = 4,
    kMaxValue = kMemoryPressure,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWarmingPoolStatus)

  // LINT.IfChange(GlicReloadAfterExpiryStatus)
  enum class ReloadAfterExpiryStatus {
    kReloaded = 0,
    kNotReloadedFeatureDisabled = 1,
    kNotReloadedLimitReached = 2,
    kMaxValue = kNotReloadedLimitReached,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicReloadAfterExpiryStatus)

  bool HasWarmedContainerForTesting() const;
  base::OneShotTimer& GetDelayTimerForTesting() { return delay_timer_; }
  WebUIContentsContainer* GetWarmedContainerForTesting() const;
  content::WebContents* GetWarmedWebContents() const;

 protected:
  class Metrics;

  // Virtual for testing.
  virtual std::unique_ptr<WebUIContentsContainer> CreateContainer();
  void OnWarmedContentCreated(ContainerCreationReason reason);

  void OnContainerExpired();
  // Starts a timer to preload a WebContents after a delay.
  void EnsurePreloadDelayed(ContainerCreationReason reason);

  // Returns true if pre-warming is permitted to run. When the stateful memory
  // pressure feature (kStatefulMemoryPressure) is enabled, pre-warming is
  // suspended while the system remains under critical memory pressure. When the
  // feature is disabled (stateless mode), pre-warming is never blocked by
  // memory pressure state.
  bool IsWarmingAllowedByMemoryPressure() const;

  raw_ptr<Profile> profile_;
  std::unique_ptr<WebUIContentsContainer> warmed_container_;

  // Timer for delayed warming.
  base::OneShotTimer delay_timer_;
  // Timer for resource cleanup.
  base::OneShotDelayedBackgroundTimer expiry_timer_;
  std::unique_ptr<Metrics> metrics_;
  // Number of times the standby container has been reloaded after expiring.
  int reload_count_ = 0;
  base::MemoryPressureLevel memory_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;
  base::TimeDelta expiry_delay_ = base::Hours(23);
  base::TimeDelta warming_delay_ = base::Seconds(20);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_
