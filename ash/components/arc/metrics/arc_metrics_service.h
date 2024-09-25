// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/metrics/arc_daily_metrics.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_wm_metrics.h"
#include "ash/components/arc/mojom/anr.mojom.h"
#include "ash/components/arc/mojom/metrics.mojom.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class BrowserContextKeyedServiceFactory;
class PrefService;

namespace metrics {
class PSIMemoryParser;
}  // namespace metrics

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcMetricsAnr;

namespace mojom {
class AppInstance;
class IntentHelperInstance;
}  // namespace mojom

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public mojom::MetricsHost,
                          public ui::GamepadObserver {
 public:
  using HistogramNamerCallback =
      base::RepeatingCallback<std::string(const std::string& base_name)>;

  class AppKillObserver : public base::CheckedObserver {
   public:
    virtual void OnArcLowMemoryKill() = 0;
    virtual void OnArcOOMKillCount(unsigned long count) = 0;
    virtual void OnArcMemoryPressureKill(int count, int estimated_freed_kb) = 0;
    virtual void OnArcMetricsServiceDestroyed() {}
  };

  class UserInteractionObserver : public base::CheckedObserver {
   public:
    virtual void OnUserInteraction(UserInteractionType type) = 0;
  };

  class BootTypeObserver : public base::CheckedObserver {
   public:
    virtual void OnBootTypeRetrieved(mojom::BootType type) = 0;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMetricsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Returns factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);

  ArcMetricsService(const ArcMetricsService&) = delete;
  ArcMetricsService& operator=(const ArcMetricsService&) = delete;

  ~ArcMetricsService() override;

  // KeyedService overrides.
  void Shutdown() override;

  // Records one of Arc.UserInteraction UMA stats. |context| cannot be null.
  static void RecordArcUserInteraction(content::BrowserContext* context,
                                       UserInteractionType type);

  // Sets the histogram namer. Required to not have a dependency on browser
  // codebase.
  void SetHistogramNamerCallback(HistogramNamerCallback histogram_namer_cb);

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;
  void ReportCompanionLibApiUsage(mojom::CompanionLibApiId api_id) override;
  void ReportDnsQueryResult(mojom::ArcDnsQuery query, bool success) override;
  void ReportAppKill(mojom::AppKillPtr app_kill) override;
  void ReportArcCorePriAbiMigEvent(
      mojom::ArcCorePriAbiMigEvent event_type) override;
  void ReportArcCorePriAbiMigFailedTries(uint32_t failed_attempts) override;
  void ReportArcCorePriAbiMigDowngradeDelay(base::TimeDelta delay) override;
  void ReportArcCorePriAbiMigBootTime(base::TimeDelta duration) override;
  void ReportArcSystemHealthUpgrade(base::TimeDelta duration,
                                    bool packages_deleted) override;
  void ReportAnr(mojom::AnrPtr anr) override;
  void ReportLowLatencyStylusLibApiUsage(
      mojom::LowLatencyStylusLibApiId api_id) override;
  void ReportLowLatencyStylusLibPredictionTarget(
      mojom::LowLatencyStylusLibPredictionTargetPtr prediction_target) override;
  void ReportVpnServiceBuilderCompatApiUsage(
      mojom::VpnServiceBuilderCompatApiId api_id) override;
  void ReportNewQosSocketCount(int count) override;
  void ReportQosSocketPercentage(int perc) override;
  void ReportMainAccountHashMigrationMetrics(
      mojom::MainAccountHashMigrationStatus status) override;
  void ReportArcNetworkEvent(mojom::ArcNetworkEvent event) override;
  void ReportArcNetworkError(mojom::ArcNetworkError error) override;
  void ReportAppPrimaryAbi(mojom::AppPrimaryAbi abi) override;
  void ReportDataRestore(mojom::DataRestoreStatus status,
                         int64_t duration_ms) override;
  void ReportMemoryPressure(const std::vector<uint8_t>& psiFile) override;
  void ReportProvisioningPreSignIn() override;
  void ReportWaylandLateTimingEvent(mojom::WaylandTimingEvent event,
                                    base::TimeDelta duration) override;
  void ReportWebViewProcessStarted() override;
  void ReportArcKeyMintError(mojom::ArcKeyMintError error) override;
  void ReportDragResizeLatency(
      const std::vector<base::TimeDelta>& durations) override;
  void ReportAppErrorDialogType(mojom::AppErrorDialogType type) override;
  void ReportApkCacheHit(bool hit) override;
  void ReportAppCategoryDataSizeList(
      std::vector<mojom::AppCategoryDataSizePtr> list) override;
  void ReportDataDirectorySizeList(
      std::vector<mojom::DataDirectorySizePtr> list) override;

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ui::GamepadObserver overrides.
  void OnGamepadEvent(const ui::GamepadEvent& event) override;

  // ArcAppListPrefs::Observer callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent);
  void OnTaskDestroyed(int32_t task_id);

  // ArcSessionManagerObserver callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnArcStarted();
  void OnArcSessionStopped();

  void AddAppKillObserver(AppKillObserver* obs);
  void RemoveAppKillObserver(AppKillObserver* obs);

  void AddUserInteractionObserver(UserInteractionObserver* obs);
  void RemoveUserInteractionObserver(UserInteractionObserver* obs);

  void AddBootTypeObserver(BootTypeObserver* obs);
  void RemoveBootTypeObserver(BootTypeObserver* obs);

  // Finds the boot_progress_arc_upgraded event, removes it from |events|, and
  // returns the event time. If the boot_progress_arc_upgraded event is not
  // found, std::nullopt is returned. This function is public for testing
  // purposes.
  std::optional<base::TimeTicks> GetArcStartTimeFromEvents(
      std::vector<mojom::BootProgressEventPtr>& events);

  // Forwards reports of app kills resulting from a MemoryPressureArcvm signal
  // to MemoryKillsMonitor via ArcMetricsServiceProxy.
  void ReportMemoryPressureArcVmKills(int count, int estimated_freed_kb);

  // Make a request to Concierge service for running VMs, then a request to
  // ArcProcessService for kill counts. Public for testing.
  void RequestKillCountsForTesting();

  void SetPrefService(PrefService* prefs);

  // Sets the UserId hash (cryptohome ID). Required to not have a dependency on
  // browser codebase.
  void set_user_id_hash(const std::string& user_id_hash) {
    user_id_hash_ = user_id_hash;
  }

  ArcDailyMetrics* get_daily_metrics_for_testing() { return daily_.get(); }

  // Record the starting time of ARC provisioning, for later use.
  void ReportProvisioningStartTime(const base::TimeTicks& start_time,
                                   const std::string& account_type_suffix);

 private:
  // Adapter to be able to also observe ProcessInstance events.
  class ProcessObserver : public ConnectionObserver<mojom::ProcessInstance> {
   public:
    explicit ProcessObserver(ArcMetricsService* arc_metrics_service);

    ProcessObserver(const ProcessObserver&) = delete;
    ProcessObserver& operator=(const ProcessObserver&) = delete;

    ~ProcessObserver() override;

   private:
    // ConnectionObserver<mojom::ProcessInstance> overrides.
    void OnConnectionReady() override;
    void OnConnectionClosed() override;

    raw_ptr<ArcMetricsService> arc_metrics_service_;
  };

  class ArcBridgeServiceObserver : public arc::ArcBridgeService::Observer {
   public:
    ArcBridgeServiceObserver();

    ArcBridgeServiceObserver(const ArcBridgeServiceObserver&) = delete;
    ArcBridgeServiceObserver& operator=(const ArcBridgeServiceObserver&) =
        delete;

    ~ArcBridgeServiceObserver() override;

    // Whether the arc bridge is in the process of closing.
    bool arc_bridge_closing_ = false;

   private:
    // arc::ArcBridgeService::Observer overrides.
    void BeforeArcBridgeClosed() override;
    void AfterArcBridgeClosed() override;
  };

  class IntentHelperObserver
      : public ConnectionObserver<mojom::IntentHelperInstance> {
   public:
    IntentHelperObserver(ArcMetricsService* arc_metrics_service,
                         ArcBridgeServiceObserver* arc_bridge_service_observer);

    IntentHelperObserver(const IntentHelperObserver&) = delete;
    IntentHelperObserver& operator=(const IntentHelperObserver&) = delete;

    ~IntentHelperObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    raw_ptr<ArcMetricsService> arc_metrics_service_;
    raw_ptr<ArcBridgeServiceObserver> arc_bridge_service_observer_;
  };

  class AppLauncherObserver : public ConnectionObserver<mojom::AppInstance> {
   public:
    AppLauncherObserver(ArcMetricsService* arc_metrics_service,
                        ArcBridgeServiceObserver* arc_bridge_service_observer);

    AppLauncherObserver(const AppLauncherObserver&) = delete;
    AppLauncherObserver& operator=(const AppLauncherObserver&) = delete;

    ~AppLauncherObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    raw_ptr<ArcMetricsService> arc_metrics_service_;
    raw_ptr<ArcBridgeServiceObserver> arc_bridge_service_observer_;
  };

  void RecordArcUserInteraction(UserInteractionType type);
  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  void OnRequestKillCountTimer();
  void OnListVmsResponse(
      std::optional<vm_tools::concierge::ListVmsResponse> response);
  void OnLowMemoryKillCounts(
      std::optional<vm_tools::concierge::ListVmsResponse> vms_list,
      mojom::LowMemoryKillCountsPtr counts);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               std::optional<base::TimeTicks> arc_start_time);
  void OnArcStartTimeForPriAbiMigration(
      base::TimeTicks durationTicks,
      std::optional<base::TimeTicks> arc_start_time);

  void OnVmsListedForKillCounts(
      std::optional<vm_tools::concierge::ListVmsResponse> response);

  // Notify AppKillObservers.
  void NotifyLowMemoryKill();
  void NotifyOOMKillCount(unsigned long count);

  // Calls sysinfo() to get the load average value and store it.
  void MeasureLoadAverage(size_t index);

  // Records load average with the appropriate histogram name if ready.
  void MaybeRecordLoadAveragePerProcessor();

  THREAD_CHECKER(thread_checker_);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // Helper class for tracking engagement metrics.
  guest_os::GuestOsEngagementMetrics guest_os_engagement_metrics_;

  // A function that appends a suffix to the base of a histogram name based on
  // the current user profile.
  HistogramNamerCallback histogram_namer_cb_;

  std::string user_id_hash_;

  ProcessObserver process_observer_;
  base::RepeatingTimer request_process_list_timer_;
  base::RepeatingTimer request_kill_count_timer_;

  mojom::LowMemoryKillCountsPtr prev_logged_memory_kills_;

  // Tracks metrics that should be logged daily. Lazily initialized in
  // SetPrefService because we need PrefService to create.
  std::unique_ptr<ArcDailyMetrics> daily_;

  ArcBridgeServiceObserver arc_bridge_service_observer_;
  IntentHelperObserver intent_helper_observer_;
  AppLauncherObserver app_launcher_observer_;
  std::unique_ptr<metrics::PSIMemoryParser> psi_parser_;

  bool was_arc_window_active_ = false;
  std::vector<int32_t> task_ids_;

  bool gamepad_interaction_recorded_ = false;

  base::ObserverList<AppKillObserver> app_kill_observers_;
  base::ObserverList<UserInteractionObserver> user_interaction_observers_;
  base::ObserverList<BootTypeObserver> boot_type_observers_;

  raw_ptr<PrefService> prefs_ = nullptr;
  std::unique_ptr<ArcMetricsAnr> metrics_anr_;

  // Tracks window management related metrics.
  std::unique_ptr<ArcWmMetrics> arc_wm_metrics_;

  // For reporting Arc.Provisioning.PreSignInTimeDelta.
  std::optional<base::TimeTicks> arc_provisioning_start_time_;
  std::optional<std::string> arc_provisioning_account_type_suffix_;

  // Load average values returned by sysinfo() after ARC start.
  // Maps from the index of the value to the value itself.
  std::map<size_t, int> load_averages_after_arc_start_;

  mojom::BootType boot_type_ = mojom::BootType::UNKNOWN;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_{this};
};

// Singleton factory for ArcMetricsService.
class ArcMetricsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsService,
          ArcMetricsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceFactory";

  static ArcMetricsServiceFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceFactory>;
  ArcMetricsServiceFactory() = default;
  ~ArcMetricsServiceFactory() override = default;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
