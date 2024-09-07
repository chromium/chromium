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
#include "content/public/browser/web_contents_user_data.h"

class ChromeBrowserMainExtraPartsPerformanceManager;
class PerformanceManagerMetricsProviderTest;
class PrefService;

namespace performance_manager::user_tuning {

// This singleton is responsible for managing the state of memory saver mode,
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
  class MemorySaverModeDelegate {
   public:
    virtual void ToggleMemorySaverMode(prefs::MemorySaverModeState state) = 0;
    virtual void SetMode(prefs::MemorySaverModeAggressiveness mode) = 0;
    virtual ~MemorySaverModeDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the memory saver mode setting is changed. Get the new
    // state using `UserPerformanceTuningManager::IsMemorySaverModeActive()`
    virtual void OnMemorySaverModeChanged() {}

    // Raised when the total memory footprint reaches X%.
    // Can be used by the UI to show a promo
    virtual void OnMemoryThresholdReached() {}

    // Raised when the tab count reaches X.
    // Can be used by the UI to show a promo
    virtual void OnTabCountThresholdReached() {}

    // Raised when the count of janky intervals reaches X.
    // Can be used by the UI to show a promo
    virtual void OnJankThresholdReached() {}
  };

  class PreDiscardResourceUsage
      : public content::WebContentsUserData<PreDiscardResourceUsage> {
   public:
    PreDiscardResourceUsage(content::WebContents* contents,
                            uint64_t memory_footprint_estimate,
                            ::mojom::LifecycleUnitDiscardReason discard_reason);
    ~PreDiscardResourceUsage() override;

    void UpdateDiscardInfo(
        uint64_t memory_footprint_estimate_kb,
        ::mojom::LifecycleUnitDiscardReason discard_reason,
        base::LiveTicks discard_live_ticks = base::LiveTicks::Now());

    // Returns the resource usage estimate in kilobytes.
    uint64_t memory_footprint_estimate_kb() const {
      return memory_footprint_estimate_;
    }

    ::mojom::LifecycleUnitDiscardReason discard_reason() const {
      return discard_reason_;
    }

    base::LiveTicks discard_live_ticks() const { return discard_live_ticks_; }

   private:
    friend WebContentsUserData;
    WEB_CONTENTS_USER_DATA_KEY_DECL();

    uint64_t memory_footprint_estimate_ = 0;
    ::mojom::LifecycleUnitDiscardReason discard_reason_;
    base::LiveTicks discard_live_ticks_;
  };

  // Returns whether a UserPerformanceTuningManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static UserPerformanceTuningManager* GetInstance();

  ~UserPerformanceTuningManager();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if Memory Saver mode is currently enabled.
  bool IsMemorySaverModeActive();

  // Returns true if the prefs underlying Memory Saver Mode are managed by an
  // enterprise policy.
  bool IsMemorySaverModeManaged() const;

  // Returns true if the prefs underlying Memory Saver Mode are still in the
  // default state.
  bool IsMemorySaverModeDefault() const;

  // Enables memory saver mode and sets the relevant prefs accordingly.
  void SetMemorySaverModeEnabled(bool enabled);

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
  };

  explicit UserPerformanceTuningManager(
      PrefService* local_state,
      std::unique_ptr<UserPerformanceTuningNotifier> notifier = nullptr,
      std::unique_ptr<MemorySaverModeDelegate> memory_saver_mode_delegate =
          nullptr);

  void Start();

  void UpdateMemorySaverModeState();
  void OnMemorySaverModePrefChanged();
  void OnMemorySaverAggressivenessPrefChanged();

  void NotifyTabCountThresholdReached();
  void NotifyMemoryThresholdReached();
  void NotifyMemoryMetricsRefreshed();

  std::unique_ptr<MemorySaverModeDelegate> memory_saver_mode_delegate_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
