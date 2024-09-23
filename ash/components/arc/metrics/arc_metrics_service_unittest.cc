// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_metrics_service.h"

#include <array>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_process_instance.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/test/test_screen.h"

namespace arc {
namespace {

// The event names the container sends to Chrome.
constexpr std::array<const char*, 11> kBootEvents{
    "boot_progress_start",
    "boot_progress_preload_start",
    "boot_progress_preload_end",
    "boot_progress_system_run",
    "boot_progress_pms_start",
    "boot_progress_pms_system_scan_start",
    "boot_progress_pms_data_scan_start",
    "boot_progress_pms_scan_end",
    "boot_progress_pms_ready",
    "boot_progress_ams_ready",
    "boot_progress_enable_screen"};

constexpr const char kBootProgressArcUpgraded[] = "boot_progress_arc_upgraded";

class ArcMetricsServiceTest : public testing::Test {
 public:
  ArcMetricsServiceTest(const ArcMetricsServiceTest&) = delete;
  ArcMetricsServiceTest& operator=(const ArcMetricsServiceTest&) = delete;

 protected:
  ArcMetricsServiceTest() : ArcMetricsServiceTest(false) {}

  explicit ArcMetricsServiceTest(bool is_arcvm_enabled) {
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    StabilityMetricsManager::Initialize(&local_state_);
    chromeos::PowerManagerClient::InitializeFake();
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
    ash::ConciergeClient::InitializeFake();

    // Changing the command line needs to be done here and not in
    // ArcVmArcMetricsServiceTest below, because we need IsArcVmEnabled to
    // return true inside the ArcMetricsService constructor in order for
    // App kill counts to be queried.
    if (is_arcvm_enabled) {
      auto* command_line = base::CommandLine::ForCurrentProcess();
      command_line->InitFromArgv({"", "--enable-arcvm"});
    }
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    // ArcMetricsService makes one call to RequestLowMemoryKillCounts when it
    // starts, so make it return 0s.
    fake_process_instance_.set_request_low_memory_kill_counts_response(
        mojom::LowMemoryKillCounts::New(0, 0, 0, 0, 0, 0, 0));
    ArcServiceManager::Get()->arc_bridge_service()->process()->SetInstance(
        &fake_process_instance_);
    context_ = std::make_unique<user_prefs::TestBrowserContextWithPrefs>();
    prefs::RegisterLocalStatePrefs(context_->pref_registry());
    prefs::RegisterProfilePrefs(context_->pref_registry());
    service_ =
        ArcMetricsService::GetForBrowserContextForTesting(context_.get());
    service_->SetPrefService(context_->prefs());

    CreateFakeWindows();
  }

  ~ArcMetricsServiceTest() override {
    fake_non_arc_window_.reset();
    fake_arc_window_.reset();

    context_.reset();
    arc_service_manager_.reset();

    ash::ConciergeClient::Shutdown();
    ash::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    StabilityMetricsManager::Shutdown();
  }

  ArcMetricsService* service() { return service_; }

  void SetArcStartTimeInMs(uint64_t arc_start_time_in_ms) {
    const base::TimeTicks arc_start_time =
        base::Milliseconds(arc_start_time_in_ms) + base::TimeTicks();
    ash::FakeSessionManagerClient::Get()->set_arc_start_time(arc_start_time);
  }

  std::vector<mojom::BootProgressEventPtr> GetBootProgressEvents(
      uint64_t start_in_ms,
      uint64_t step_in_ms) {
    std::vector<mojom::BootProgressEventPtr> events;
    for (size_t i = 0; i < kBootEvents.size(); ++i) {
      events.emplace_back(mojom::BootProgressEvent::New(
          kBootEvents[i], start_in_ms + (step_in_ms * i)));
    }
    return events;
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  aura::Window* fake_arc_window() { return fake_arc_window_.get(); }
  aura::Window* fake_non_arc_window() { return fake_non_arc_window_.get(); }

  FakeProcessInstance& process_instance() { return fake_process_instance_; }

  PrefService* prefs() { return context_->prefs(); }

 private:
  void CreateFakeWindows() {
    fake_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/0, nullptr));
    fake_arc_window_->SetProperty(chromeos::kAppTypeKey,
                                  chromeos::AppType::ARC_APP);
    fake_non_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/1, nullptr));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};

  TestingPrefServiceSimple local_state_;
  session_manager::SessionManager session_manager_;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<user_prefs::TestBrowserContextWithPrefs> context_;
  raw_ptr<ArcMetricsService, DanglingUntriaged> service_;

  std::unique_ptr<aura::Window> fake_arc_window_;
  std::unique_ptr<aura::Window> fake_non_arc_window_;

  FakeProcessInstance fake_process_instance_;
};

// Tests that ReportBootProgress() actually records UMA stats.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBoot) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 10,
  // boot_progress_preload_start to 11, and so on.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 1 (ms), etc.
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  // Confirm that Arc.AndroidBootTime.FirstBoot is recorded.
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 1);
}

// Does the same but with negative values and FIRST_BOOT_AFTER_UPDATE.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBootAfterUpdate) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 5,
  // boot_progress_preload_start to 7, and so on. This can actually happen
  // because the mini container can finish up to boot_progress_preload_end
  // before the full container is started.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 0 (ms), etc. Unlike our
  // performance dashboard where negative performance numbers are treated as-is,
  // UMA treats them as zeros.
  base::HistogramTester tester;
  // This time, use mojom::BootType::FIRST_BOOT_AFTER_UPDATE.
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::FIRST_BOOT_AFTER_UPDATE);
  base::RunLoop().RunUntilIdle();
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 0);
}

// Does the same but with REGULAR_BOOT.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_RegularBoot) {
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::REGULAR_BOOT);
  base::RunLoop().RunUntilIdle();
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 0);
}

// Tests that no UMA is recorded when nothing is reported.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_EmptyResults) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events;  // empty

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 0);
}

// Tests that no UMA is recorded when BootType is invalid.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_InvalidBootType) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(123, 456));
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::UNKNOWN);
  base::RunLoop().RunUntilIdle();
  for (const std::string& suffix :
       {".FirstBoot", ".FirstBootAfterUpdate", ".RegularBoot"}) {
    tester.ExpectTotalCount("Arc.AndroidBootTime" + suffix, 0);
  }
}

TEST_F(ArcMetricsServiceTest, RecordLoadAveragePerProcessor) {
  service()->OnArcStarted();
  service()->ReportBootProgress({}, mojom::BootType::REGULAR_BOOT);
  base::HistogramTester tester;
  FastForwardBy(base::Minutes(15));
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor1MinuteAfterArcStart.RegularBoot", 1);
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor5MinutesAfterArcStart.RegularBoot", 1);
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor15MinutesAfterArcStart.RegularBoot", 1);
}

// Tests that load average histograms are recorded even if ReportBootProgress()
// is called after measuring the load average values.
TEST_F(ArcMetricsServiceTest,
       RecordLoadAveragePerProcessor_LateReportBootProgress) {
  service()->OnArcStarted();
  base::HistogramTester tester;
  FastForwardBy(base::Minutes(2));
  service()->ReportBootProgress({}, mojom::BootType::REGULAR_BOOT);
  FastForwardBy(base::Minutes(15));
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor1MinuteAfterArcStart.RegularBoot", 1);
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor5MinutesAfterArcStart.RegularBoot", 1);
  tester.ExpectTotalCount(
      "Arc.LoadAverageX100PerProcessor15MinutesAfterArcStart.RegularBoot", 1);
}

TEST_F(ArcMetricsServiceTest, ReportNativeBridge) {
  // SetArcNativeBridgeType should be called once ArcMetricsService is
  // constructed.
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::UNKNOWN);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NONE);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NONE);
  service()->ReportNativeBridge(mojom::NativeBridgeType::HOUDINI);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::HOUDINI);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NDK_TRANSLATION);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NDK_TRANSLATION);
}

TEST_F(ArcMetricsServiceTest, RecordArcWindowFocusAction) {
  base::HistogramTester tester;

  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, RecordNothingNonArcWindowFocusAction) {
  base::HistogramTester tester;

  // Focus an ARC window once so that the histogram is created.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);

  // Focusing a non-ARC window should not increase the bucket count.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_non_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, GetArcStartTimeFromEvents) {
  constexpr uint64_t kArcStartTimeMs = 10;
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));
  events.emplace_back(
      mojom::BootProgressEvent::New(kBootProgressArcUpgraded, kArcStartTimeMs));

  std::optional<base::TimeTicks> arc_start_time =
      service()->GetArcStartTimeFromEvents(events);
  EXPECT_TRUE(arc_start_time.has_value());
  EXPECT_EQ(*arc_start_time, base::Milliseconds(10) + base::TimeTicks());

  // Check that the upgrade event was removed from events.
  EXPECT_TRUE(
      base::ranges::none_of(events, [](const mojom::BootProgressEventPtr& ev) {
        return ev->event.compare(kBootProgressArcUpgraded) == 0;
      }));
}

TEST_F(ArcMetricsServiceTest, GetArcStartTimeFromEvents_NoArcUpgradedEvent) {
  constexpr uint64_t kArcStartTimeMs = 10;
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));

  std::optional<base::TimeTicks> arc_start_time =
      service()->GetArcStartTimeFromEvents(events);
  EXPECT_FALSE(arc_start_time.has_value());
}

TEST_F(ArcMetricsServiceTest, UserInteractionObserver) {
  class Observer : public ArcMetricsService::UserInteractionObserver {
   public:
    void OnUserInteraction(UserInteractionType interaction_type) override {
      type = interaction_type;
    }
    std::optional<UserInteractionType> type;
  } observer;

  service()->AddUserInteractionObserver(&observer);

  // This calls RecordArcUserInteraction() with APP_CONTENT_WINDOW_INTERACTION.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  ASSERT_TRUE(observer.type);
  EXPECT_EQ(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION,
            *observer.type);

  service()->RemoveUserInteractionObserver(&observer);
}

TEST_F(ArcMetricsServiceTest, BootTypeObserver) {
  class Observer : public ArcMetricsService::BootTypeObserver {
   public:
    void OnBootTypeRetrieved(mojom::BootType type) override { type_ = type; }

    std::optional<mojom::BootType> type_;
  } observer;

  service()->AddBootTypeObserver(&observer);

  service()->ReportBootProgress({}, mojom::BootType::FIRST_BOOT_AFTER_UPDATE);
  EXPECT_EQ(mojom::BootType::FIRST_BOOT_AFTER_UPDATE, observer.type_);

  service()->RemoveBootTypeObserver(&observer);
}

TEST_F(ArcMetricsServiceTest, ReportWebViewProcessStarted_NoUsageReported) {
  base::HistogramTester tester;

  service()->OnArcSessionStopped();

  tester.ExpectUniqueSample("Arc.Session.HasWebViewUsage",
                            static_cast<base::HistogramBase::Sample>(0), 1);
}

TEST_F(ArcMetricsServiceTest, ReportWebViewProcessStarted_OneUsageReported) {
  base::HistogramTester tester;

  service()->ReportWebViewProcessStarted();
  service()->OnArcSessionStopped();

  tester.ExpectUniqueSample("Arc.Session.HasWebViewUsage",
                            static_cast<base::HistogramBase::Sample>(1), 1);
}

TEST_F(ArcMetricsServiceTest, ReportWebViewProcessStarted_SomeUsageReported) {
  base::HistogramTester tester;

  // 3 sessions with webview reported in 2 sessions.
  service()->ReportWebViewProcessStarted();
  service()->OnArcSessionStopped();

  service()->OnArcSessionStopped();

  service()->ReportWebViewProcessStarted();
  service()->OnArcSessionStopped();

  tester.ExpectBucketCount("Arc.Session.HasWebViewUsage",
                           static_cast<base::HistogramBase::Sample>(0), 1);
  tester.ExpectBucketCount("Arc.Session.HasWebViewUsage",
                           static_cast<base::HistogramBase::Sample>(1), 2);
}

TEST_F(ArcMetricsServiceTest, ReportArcKeyMintError_SomeErrorReported) {
  base::HistogramTester tester;

  service()->ReportArcKeyMintError(arc::mojom::ArcKeyMintError::kUnknownError);
  service()->OnArcSessionStopped();

  tester.ExpectUniqueSample("Arc.KeyMint.KeyMintError",
                            static_cast<base::HistogramBase::Sample>(2), 1);
}

TEST_F(ArcMetricsServiceTest, ReportVpnServiceBuilderCompatApiUsage) {
  base::HistogramTester tester;

  service()->ReportVpnServiceBuilderCompatApiUsage(
      mojom::VpnServiceBuilderCompatApiId::kVpnExcludeRoute);
  service()->ReportVpnServiceBuilderCompatApiUsage(
      mojom::VpnServiceBuilderCompatApiId::kVpnAddRoute);

  tester.ExpectBucketCount(
      "Arc.VpnServiceBuilderCompatApisCounter",
      static_cast<int>(mojom::VpnServiceBuilderCompatApiId::kVpnExcludeRoute),
      1);
  tester.ExpectBucketCount(
      "Arc.VpnServiceBuilderCompatApisCounter",
      static_cast<int>(mojom::VpnServiceBuilderCompatApiId::kVpnAddRoute), 1);
}

// Tests that ReportApkCacheHit() actually records UMA stats.
TEST_F(ArcMetricsServiceTest, ReportApkCacheHit) {
  base::HistogramTester tester;

  service()->ReportApkCacheHit(true /*hit*/);
  tester.ExpectUniqueSample("Arc.AppInstall.CacheHit", 1, 1);

  service()->ReportApkCacheHit(false /*hit*/);
  tester.ExpectBucketCount("Arc.AppInstall.CacheHit", 0, 1);

  tester.ExpectTotalCount("Arc.AppInstall.CacheHit", 2);
}

class ArcVmArcMetricsServiceTest
    : public ArcMetricsServiceTest,
      public testing::WithParamInterface<
          std::optional<vm_tools::concierge::ListVmsResponse>> {
 public:
  ArcVmArcMetricsServiceTest(const ArcVmArcMetricsServiceTest&) = delete;
  ArcVmArcMetricsServiceTest& operator=(const ArcVmArcMetricsServiceTest&) =
      delete;

 protected:
  ArcVmArcMetricsServiceTest() : ArcMetricsServiceTest(true) {}

  void RequestKillCountsAndRespond(
      mojom::LowMemoryKillCountsPtr counts,
      std::optional<vm_tools::concierge::ListVmsResponse> list_vms_response) {
    ash::FakeConciergeClient::Get()->set_list_vms_response(
        std::move(list_vms_response));
    process_instance().set_request_low_memory_kill_counts_response(
        std::move(counts));
    service()->RequestKillCountsForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TriggerDailyEvent() {
    auto* daily_metrics = service()->get_daily_metrics_for_testing();
    // Clear DailyEvent, so that it looks in prefs for the timestamp.
    daily_metrics->SetDailyEventForTesting(
        std::make_unique<metrics::DailyEvent>(
            prefs(), prefs::kArcDailyMetricsSample,
            ArcDailyMetrics::kDailyEventHistogramName));
    base::Time last_time = base::Time::Now() - base::Hours(25);
    prefs()->SetInt64(prefs::kArcDailyMetricsSample,
                      last_time.since_origin().InMicroseconds());
    daily_metrics->get_daily_event_for_testing()->CheckInterval();
  }
};

// Create a ListVmsResponse used to create the VM specific memory counters.
// See LogVmSpecificLowMemoryKillCounts.
static std::optional<vm_tools::concierge::ListVmsResponse> VmsList(
    std::initializer_list<vm_tools::concierge::VmInfo_VmType> types) {
  // ArcMetricsService only uses the vm_type field and ignores everything else,
  // so that's the only thing we need to set.
  auto list = vm_tools::concierge::ListVmsResponse();
  list.set_success(true);
  for (auto type : types) {
    auto* info = list.add_vms();
    info->mutable_vm_info()->set_vm_type(type);
  }
  return list;
}

struct KillCounterInfo {
  const char* name;
  uint32_t mojom::LowMemoryKillCounts::*const member;
};

// Store a list of the different kill counter names and which field in the
// mojo structure holds them.
static constexpr std::array<KillCounterInfo, 7> kKillCounterInfo = {{
    {"LinuxOOM", &mojom::LowMemoryKillCounts::guest_oom},
    {"LMKD.Foreground", &mojom::LowMemoryKillCounts::lmkd_foreground},
    {"LMKD.Perceptible", &mojom::LowMemoryKillCounts::lmkd_perceptible},
    {"LMKD.Cached", &mojom::LowMemoryKillCounts::lmkd_cached},
    {"Pressure.Foreground", &mojom::LowMemoryKillCounts::pressure_foreground},
    {"Pressure.Perceptible", &mojom::LowMemoryKillCounts::pressure_perceptible},
    {"Pressure.Cached", &mojom::LowMemoryKillCounts::pressure_cached},
}};

typedef vm_tools::concierge::VmInfo_VmType VmType;

static constexpr VmType VmType_ARC_VM =
    vm_tools::concierge::VmInfo_VmType_ARC_VM;
static constexpr VmType VmType_BOREALIS =
    vm_tools::concierge::VmInfo_VmType_BOREALIS;
static constexpr VmType VmType_PLUGIN_VM =
    vm_tools::concierge::VmInfo_VmType_PLUGIN_VM;
static constexpr VmType VmType_TERMINA =
    vm_tools::concierge::VmInfo_VmType_TERMINA;
static constexpr VmType VmType_UNKNOWN =
    vm_tools::concierge::VmInfo_VmType_UNKNOWN;

static const char* VmKillCounterPrefix(VmType vm) {
  switch (vm) {
    case VmType_ARC_VM:
      // We assume the caller has checked that no other VM is running.
      return ".OnlyArc";

    case VmType_BOREALIS:
      return ".Steam";

    case VmType_PLUGIN_VM:
      return ".PluginVm";

    case VmType_TERMINA:
      return ".Crostini";

    default:
      return ".UnknownVm";
  }
}

INSTANTIATE_TEST_SUITE_P(
    MultiVm,
    ArcVmArcMetricsServiceTest,
    testing::Values(std::nullopt,
                    VmsList({}),
                    VmsList({VmType_ARC_VM}),
                    VmsList({VmType_ARC_VM, VmType_BOREALIS}),
                    VmsList({VmType_ARC_VM, VmType_TERMINA}),
                    VmsList({VmType_ARC_VM, VmType_UNKNOWN}),
                    VmsList({VmType_ARC_VM, VmType_PLUGIN_VM}),
                    VmsList({VmType_ARC_VM, VmType_BOREALIS, VmType_PLUGIN_VM,
                             VmType_TERMINA, VmType_UNKNOWN})));

static void ExpectNoAppKillCountsForVm(base::HistogramTester& tester,
                                       const char* vm_prefix) {
  for (const auto counter : kKillCounterInfo) {
    const auto name = base::StringPrintf(
        "Arc.App.LowMemoryKills%s.%sCount10Minutes", vm_prefix, counter.name);
    tester.ExpectTotalCount(name, 0);
  }
}

static void ExpectNoAppKillCounts(base::HistogramTester& tester) {
  ExpectNoAppKillCountsForVm(tester, "");
  ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_ARC_VM));
  ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_BOREALIS));
  ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_PLUGIN_VM));
  ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_TERMINA));
  ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_UNKNOWN));
}

void ExpectOneSampleAppKillCountsForVm(
    base::HistogramTester& tester,
    const char* vm_prefix,
    const mojom::LowMemoryKillCountsPtr& c0,
    const mojom::LowMemoryKillCountsPtr& c1) {
  for (const auto counter : kKillCounterInfo) {
    const auto name = base::StringPrintf(
        "Arc.App.LowMemoryKills%s.%sCount10Minutes", vm_prefix, counter.name);
    base::Histogram::Count value =
        (*c1).*(counter.member) - (*c0).*(counter.member);
    tester.ExpectUniqueSample(name, value, 1);
  }
}

void ExpectOneSampleAppKillCounts(
    base::HistogramTester& tester,
    std::optional<vm_tools::concierge::ListVmsResponse> vms,
    const mojom::LowMemoryKillCountsPtr& c0,
    const mojom::LowMemoryKillCountsPtr& c1) {
  // No VM prefix for general counters.
  ExpectOneSampleAppKillCountsForVm(tester, "", c0, c1);

  // VM specific counters. First build a set of the running VMs.
  std::unordered_set<VmType> running;
  if (vms) {
    for (int i = 0; i < vms->vms_size(); i++) {
      const auto& vm = vms->vms(i);
      if (!vm.has_vm_info()) {
        continue;
      }
      running.insert(vm.vm_info().vm_type());
    }
  }

  // ARCVM is special, because we only increment those counters if it's the only
  // VM.
  if (running.count(VmType_ARC_VM) == 1 && running.size() == 1) {
    ExpectOneSampleAppKillCountsForVm(
        tester, VmKillCounterPrefix(VmType_ARC_VM), c0, c1);
  } else {
    ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(VmType_ARC_VM));
  }
  // Other VM counters should only incremented if that VM is running.
  std::initializer_list<VmType> other_vms = {VmType_BOREALIS, VmType_PLUGIN_VM,
                                             VmType_TERMINA, VmType_UNKNOWN};
  for (auto vm : other_vms) {
    if (running.count(vm) == 1) {
      ExpectOneSampleAppKillCountsForVm(tester, VmKillCounterPrefix(vm), c0,
                                        c1);
    } else {
      ExpectNoAppKillCountsForVm(tester, VmKillCounterPrefix(vm));
    }
  }
}

TEST_P(ArcVmArcMetricsServiceTest, AppLowMemoryKills) {
  // The test code sets the initial counts to 0.
  auto c0 = mojom::LowMemoryKillCounts::New(0, 0, 0, 0, 0, 0, 0);
  // First sample counts.
  auto c1 = mojom::LowMemoryKillCounts::New(1,   // oom.
                                            2,   // lmkd_foreground.
                                            3,   // lmkd_perceptible.
                                            4,   // lmkd_cached.
                                            5,   // pressure_foreground.
                                            6,   // pressure_perceptible.
                                            7);  // pressure_cached.
  // Second sample counts.
  auto c2 = mojom::LowMemoryKillCounts::New(17,   // oom.
                                            16,   // lmkd_foreground.
                                            15,   // lmkd_perceptible.
                                            14,   // lmkd_cached.
                                            13,   // pressure_foreground.
                                            12,   // pressure_perceptible.
                                            11);  // pressure_cached.
  // Third sample counts all decrease by 1.
  auto c3 = mojom::LowMemoryKillCounts::New(16,   // oom.
                                            15,   // lmkd_foreground.
                                            14,   // lmkd_perceptible.
                                            13,   // lmkd_cached.
                                            12,   // pressure_foreground.
                                            11,   // pressure_perceptible.
                                            10);  // pressure_cached.

  {
    base::HistogramTester tester;
    RequestKillCountsAndRespond(c1->Clone(), GetParam());
    ExpectOneSampleAppKillCounts(tester, GetParam(), c0, c1);
  }

  {
    base::HistogramTester tester;
    RequestKillCountsAndRespond(c2->Clone(), GetParam());
    ExpectOneSampleAppKillCounts(tester, GetParam(), c1, c2);
  }

  {
    base::HistogramTester tester;
    RequestKillCountsAndRespond(c3->Clone(), GetParam());
    // Counts decreased, so expect no samples.
    ExpectNoAppKillCounts(tester);
  }
}

static void ExpectOneSampleAppKillDailyCountsForVm(
    base::HistogramTester& tester,
    const char* vm_prefix,
    int oom,
    int foreground,
    int perceptible,
    int cached) {
  tester.ExpectUniqueSample(
      base::StringPrintf("Arc.App.LowMemoryKills%s.OomDaily", vm_prefix), oom,
      1);
  tester.ExpectUniqueSample(
      base::StringPrintf("Arc.App.LowMemoryKills%s.ForegroundDaily", vm_prefix),
      foreground, 1);
  tester.ExpectUniqueSample(
      base::StringPrintf("Arc.App.LowMemoryKills%s.PerceptibleDaily",
                         vm_prefix),
      perceptible, 1);
  tester.ExpectUniqueSample(
      base::StringPrintf("Arc.App.LowMemoryKills%s.CachedDaily", vm_prefix),
      cached, 1);
}

static void ExpectOneSampleAppKillDailyCounts(
    base::HistogramTester& tester,
    std::optional<vm_tools::concierge::ListVmsResponse> vms,
    int oom,
    int foreground,
    int perceptible,
    int cached) {
  // No VM prefix for total counters.
  ExpectOneSampleAppKillDailyCountsForVm(tester, "", oom, foreground,
                                         perceptible, cached);

  // VM specific counters. First build a set of the running VMs.
  std::unordered_set<VmType> running;
  if (vms) {
    for (int i = 0; i < vms->vms_size(); i++) {
      const auto& vm = vms->vms(i);
      if (!vm.has_vm_info()) {
        continue;
      }
      running.insert(vm.vm_info().vm_type());
    }
  }

  // ARCVM is special, because we only increment those counters if it's the only
  // VM.
  if (running.count(VmType_ARC_VM) == 1 && running.size() == 1) {
    ExpectOneSampleAppKillDailyCountsForVm(
        tester, VmKillCounterPrefix(VmType_ARC_VM), oom, foreground,
        perceptible, cached);
  } else {
    ExpectOneSampleAppKillDailyCountsForVm(
        tester, VmKillCounterPrefix(VmType_ARC_VM), 0, 0, 0, 0);
  }
  // Other VM counters should only incremented if that VM is running.
  std::initializer_list<VmType> other_vms = {VmType_BOREALIS, VmType_PLUGIN_VM,
                                             VmType_TERMINA, VmType_UNKNOWN};
  for (auto vm : other_vms) {
    if (running.count(vm) == 1) {
      ExpectOneSampleAppKillDailyCountsForVm(tester, VmKillCounterPrefix(vm),
                                             oom, foreground, perceptible,
                                             cached);
    } else {
      ExpectOneSampleAppKillDailyCountsForVm(tester, VmKillCounterPrefix(vm), 0,
                                             0, 0, 0);
    }
  }
}

TEST_P(ArcVmArcMetricsServiceTest, AppLowMemoryDailyKills) {
  printf("GetParam() VMs:");
  if (GetParam()) {
    for (int i = 0; i < GetParam()->vms_size(); i++) {
      const auto& vm = GetParam()->vms(i);
      if (!vm.has_vm_info()) {
        continue;
      }
      printf(" %s", VmKillCounterPrefix(vm.vm_info().vm_type()));
    }
  }

  printf("\n");

  // The test code sets the initial counts to 0.
  auto c0 = mojom::LowMemoryKillCounts::New(0, 0, 0, 0, 0, 0, 0);
  // First sample counts.
  auto c1 = mojom::LowMemoryKillCounts::New(1,   // oom.
                                            2,   // lmkd_foreground.
                                            3,   // lmkd_perceptible.
                                            4,   // lmkd_cached.
                                            5,   // pressure_foreground.
                                            6,   // pressure_perceptible.
                                            7);  // pressure_cached.
  // Second sample counts.
  auto c2 = mojom::LowMemoryKillCounts::New(17,   // oom.
                                            16,   // lmkd_foreground.
                                            15,   // lmkd_perceptible.
                                            14,   // lmkd_cached.
                                            13,   // pressure_foreground.
                                            12,   // pressure_perceptible.
                                            11);  // pressure_cached.

  // Third sample is for a second day.
  auto c3 = mojom::LowMemoryKillCounts::New(18,   // oom.
                                            18,   // lmkd_foreground.
                                            18,   // lmkd_perceptible.
                                            18,   // lmkd_cached.
                                            18,   // pressure_foreground.
                                            18,   // pressure_perceptible.
                                            18);  // pressure_cached.

  RequestKillCountsAndRespond(c0->Clone(), std::nullopt);
  RequestKillCountsAndRespond(c1->Clone(), GetParam());
  RequestKillCountsAndRespond(c2->Clone(), std::nullopt);
  // Reset daily events to make sure we restore values from prefs.
  // NB: We make a new ArcDailyMetrics for the passed prefs in SetPrefService.
  service()->SetPrefService(prefs());

  {
    base::HistogramTester tester;
    TriggerDailyEvent();
    ExpectOneSampleAppKillDailyCounts(tester, GetParam(), 17, 29, 27, 25);
  }

  RequestKillCountsAndRespond(c3->Clone(), GetParam());

  {
    base::HistogramTester tester;
    TriggerDailyEvent();
    ExpectOneSampleAppKillDailyCounts(tester, GetParam(), 1, 7, 9, 11);
  }
}

}  // namespace
}  // namespace arc
