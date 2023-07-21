// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ChromeBrowserMainExtraPartsPerformanceManager;
class PerformanceManagerMetricsProviderTest;
class PrefService;

namespace performance_manager::user_tuning {

// This singleton is responsible for managing the state of high efficiency mode,
// as well as the different signals surrounding its toggling.
//
// It is created and owned by `ChromeBrowserMainExtraPartsPerformanceManager`
// and initialized in 2 parts:
// - Created in PostCreateThreads (so that UI can start observing it as soon as
// the first views are created) and
// - Starts to manage the mode when Start() is called in PreMainMessageLoopRun.
//
// This object lives on the main thread and should be used from it exclusively.
class UserPerformanceTuningManager {
 public:
  class HighEfficiencyModeDelegate {
   public:
    virtual void ToggleHighEfficiencyMode(
        prefs::HighEfficiencyModeState state) = 0;
    virtual void SetTimeBeforeDiscard(base::TimeDelta time_before_discard) = 0;
    virtual ~HighEfficiencyModeDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the high efficiency mode setting is changed. Get the new
    // state using `UserPerformanceTuningManager::IsHighEfficiencyModeActive()`
    virtual void OnHighEfficiencyModeChanged() {}

    // Raised when the total memory footprint reaches X%.
    // Can be used by the UI to show a promo
    virtual void OnMemoryThresholdReached() {}

    // Raised when the tab count reaches X.
    // Can be used by the UI to show a promo
    virtual void OnTabCountThresholdReached() {}

    // Raised when the count of janky intervals reaches X.
    // Can be used by the UI to show a promo
    virtual void OnJankThresholdReached() {}

    // Raised when memory metrics for a discarded page becomes available to read
    virtual void OnMemoryMetricsRefreshed() {}
  };

  class TabResourceUsage : public base::RefCounted<TabResourceUsage> {
   public:
    TabResourceUsage() = default;

    uint64_t memory_usage_in_bytes() const { return memory_usage_bytes_; }

    void set_memory_usage_in_bytes(uint64_t memory_usage_bytes) {
      memory_usage_bytes_ = memory_usage_bytes;
    }

   private:
    friend class base::RefCounted<TabResourceUsage>;
    ~TabResourceUsage() = default;

    uint64_t memory_usage_bytes_ = 0;
  };

  // Per-tab class to keep track of current memory usage for each tab.
  class ResourceUsageTabHelper
      : public content::WebContentsObserver,
        public content::WebContentsUserData<ResourceUsageTabHelper> {
   public:
    ResourceUsageTabHelper(const ResourceUsageTabHelper&) = delete;
    ResourceUsageTabHelper& operator=(const ResourceUsageTabHelper&) = delete;

    ~ResourceUsageTabHelper() override;

    // content::WebContentsObserver
    void PrimaryPageChanged(content::Page& page) override;

    uint64_t GetMemoryUsageInBytes() {
      return resource_usage_->memory_usage_in_bytes();
    }

    void SetMemoryUsageInBytes(uint64_t memory_usage_bytes) {
      resource_usage_->set_memory_usage_in_bytes(memory_usage_bytes);
    }

    scoped_refptr<const TabResourceUsage> resource_usage() const {
      return resource_usage_;
    }

   private:
    friend class content::WebContentsUserData<ResourceUsageTabHelper>;
    explicit ResourceUsageTabHelper(content::WebContents* contents);
    WEB_CONTENTS_USER_DATA_KEY_DECL();

    scoped_refptr<TabResourceUsage> resource_usage_;
  };

  class PreDiscardResourceUsage
      : public content::WebContentsUserData<PreDiscardResourceUsage> {
   public:
    PreDiscardResourceUsage(content::WebContents* contents,
                            uint64_t memory_footprint_estimate,
                            ::mojom::LifecycleUnitDiscardReason discard_reason);
    ~PreDiscardResourceUsage() override;

    // Returns the resource usage estimate in kilobytes.
    uint64_t memory_footprint_estimate_kb() const {
      return memory_footprint_estimate_;
    }

    void SetMemoryFootprintEstimateKbForTesting(
        uint64_t memory_footprint_estimate) {
      memory_footprint_estimate_ = memory_footprint_estimate;
    }

    ::mojom::LifecycleUnitDiscardReason discard_reason() const {
      return discard_reason_;
    }

    base::LiveTicks discard_liveticks() const { return discard_liveticks_; }

   private:
    friend WebContentsUserData;
    WEB_CONTENTS_USER_DATA_KEY_DECL();

    uint64_t memory_footprint_estimate_ = 0;
    ::mojom::LifecycleUnitDiscardReason discard_reason_;
    base::LiveTicks discard_liveticks_;
  };

  // Returns whether a UserPerformanceTuningManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static UserPerformanceTuningManager* GetInstance();

  ~UserPerformanceTuningManager();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if High Efficiency mode is currently enabled.
  bool IsHighEfficiencyModeActive();

  // Returns true if the prefs underlying High Efficiency Mode are managed by an
  // enterprise policy.
  bool IsHighEfficiencyModeManaged() const;

  // Returns true if the prefs underlying High Efficiency Mode are still in the
  // default state.
  bool IsHighEfficiencyModeDefault() const;

  // Enables high efficiency mode and sets the relevant prefs accordingly.
  void SetHighEfficiencyModeEnabled(bool enabled);

  // Discards the given WebContents with the same mechanism as one that is
  // discarded through a natural timeout
  void DiscardPageForTesting(content::WebContents* web_contents);

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class ::PerformanceManagerMetricsProviderTest;
  friend class UserPerformanceTuningManagerTest;
  friend class TestUserPerformanceTuningManagerEnvironment;

  // An implementation of UserPerformanceTuningNotifier::Receiver that
  // forwards the notifications to the UserPerformanceTuningManager on the Main
  // Thread.
  class UserPerformanceTuningReceiverImpl
      : public UserPerformanceTuningNotifier::Receiver {
   public:
    ~UserPerformanceTuningReceiverImpl() override;

    void NotifyTabCountThresholdReached() override;
    void NotifyMemoryThresholdReached() override;
    void NotifyMemoryMetricsRefreshed(ProxyAndPmfKbVector) override;
  };

  explicit UserPerformanceTuningManager(
      PrefService* local_state,
      std::unique_ptr<UserPerformanceTuningNotifier> notifier = nullptr,
      std::unique_ptr<HighEfficiencyModeDelegate>
          high_efficiency_mode_delegate = nullptr);

  void Start();

  void OnHighEfficiencyModePrefChanged();
  void OnHighEfficiencyModeTimeBeforeDiscardChanged();

  void NotifyTabCountThresholdReached();
  void NotifyMemoryThresholdReached();
  void NotifyMemoryMetricsRefreshed();

  std::unique_ptr<HighEfficiencyModeDelegate> high_efficiency_mode_delegate_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
