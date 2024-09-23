// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/device_storage_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/launcher_search/system_info/launcher_util.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/system_info/cpu_data.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/components/system_info/system_info_util.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"

namespace app_list::test {
namespace {

namespace healthd_mojom = ash::cros_healthd::mojom;

constexpr char kBatteryDataError[] =
    "Apps.AppList.SystemInfoProvider.Error.Battery";

constexpr char kProbeErrorBatteryInfo[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.BatteryInfo";
constexpr char kProbeErrorCpuInfo[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.CpuInfo";
constexpr char kProbeErrorMemoryInfo[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.MemoryInfo";

void SetProbeTelemetryInfoResponse(healthd_mojom::BatteryInfoPtr battery_info,
                                   healthd_mojom::CpuInfoPtr cpu_info,
                                   healthd_mojom::MemoryInfoPtr memory_info) {
  auto info = healthd_mojom::TelemetryInfo::New();
  if (battery_info) {
    info->battery_result =
        healthd_mojom::BatteryResult::NewBatteryInfo(std::move(battery_info));
  }
  if (memory_info) {
    info->memory_result =
        healthd_mojom::MemoryResult::NewMemoryInfo(std::move(memory_info));
  }
  if (cpu_info) {
    info->cpu_result =
        healthd_mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
  }

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);
}

void SetCrosHealthdCpuResponse(
    const std::vector<system_info::CpuUsageData>& usage_data,
    const std::vector<int32_t>& cpu_temps,
    const std::vector<uint32_t>& scaled_cpu_clock_speed) {
  auto cpu_info_ptr = healthd_mojom::CpuInfo::New();
  auto physical_cpu_info_ptr = healthd_mojom::PhysicalCpuInfo::New();

  DCHECK_EQ(usage_data.size(), scaled_cpu_clock_speed.size());
  for (size_t i = 0; i < usage_data.size(); ++i) {
    const auto& data = usage_data[i];
    auto logical_cpu_info_ptr = healthd_mojom::LogicalCpuInfo::New();

    logical_cpu_info_ptr->user_time_user_hz = data.GetUserTime();
    logical_cpu_info_ptr->system_time_user_hz = data.GetSystemTime();
    logical_cpu_info_ptr->idle_time_user_hz = data.GetIdleTime();

    logical_cpu_info_ptr->scaling_current_frequency_khz =
        scaled_cpu_clock_speed[i];

    physical_cpu_info_ptr->logical_cpus.emplace_back(
        std::move(logical_cpu_info_ptr));
  }

  cpu_info_ptr->physical_cpus.push_back(std::move(physical_cpu_info_ptr));
  for (const auto& cpu_temp : cpu_temps) {
    auto cpu_temp_channel_ptr = healthd_mojom::CpuTemperatureChannel::New();
    cpu_temp_channel_ptr->temperature_celsius = cpu_temp;
    cpu_info_ptr->temperature_channels.emplace_back(
        std::move(cpu_temp_channel_ptr));
  }

  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                std::move(cpu_info_ptr),
                                /*memory_info=*/nullptr);
}

void SetCrosHealthdMemoryUsageResponse(uint32_t total_memory_kib,
                                       uint32_t free_memory_kib,
                                       uint32_t available_memory_kib) {
  healthd_mojom::MemoryInfoPtr memory_info = healthd_mojom::MemoryInfo::New(
      total_memory_kib, free_memory_kib, available_memory_kib,
      /*page_faults_since_last_boot=*/0);
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr, /*cpu_info=*/nullptr,
                                /*memory_info=*/std::move(memory_info));
}

// Constructs a BatteryInfoPtr.
healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryHealthResponse(
    double charge_full_now,
    double charge_full_design,
    int32_t cycle_count) {
  healthd_mojom::NullableUint64Ptr temp_value_ptr(
      healthd_mojom::NullableUint64::New());
  auto battery_info = healthd_mojom::BatteryInfo::New(
      /*cycle_count=*/cycle_count, /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"", /*charge_full_design=*/charge_full_design,
      /*charge_full=*/charge_full_now,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/0,
      /*current_now=*/0,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/std::nullopt, std::move(temp_value_ptr));
  return battery_info;
}

void SetCrosHealthdBatteryHealthResponse(double charge_full_now,
                                         double charge_full_design,
                                         int32_t cycle_count) {
  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryHealthResponse(charge_full_now,
                                             charge_full_design, cycle_count);
  SetProbeTelemetryInfoResponse(std::move(battery_info),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr);
}

bool AreValidPowerTimes(int64_t time_to_full, int64_t time_to_empty) {
  // Exactly one of `time_to_full` or `time_to_empty` must be zero. The other
  // can be a positive integer to represent the time to charge/discharge or -1
  // to represent that the time is being calculated.
  return (time_to_empty == 0 && (time_to_full > 0 || time_to_full == -1)) ||
         (time_to_full == 0 && (time_to_empty > 0 || time_to_empty == -1));
}

power_manager::PowerSupplyProperties ConstructPowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty,
    double battery_percent) {
  power_manager::PowerSupplyProperties props;
  props.set_external_power(power_source);
  props.set_battery_state(battery_state);

  if (battery_state ==
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    // Leave `time_to_full` and `time_to_empty` unset.
    return props;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  props.set_is_calculating_battery_time(is_calculating_battery_time);
  props.set_battery_time_to_full_sec(time_to_full);
  props.set_battery_time_to_empty_sec(time_to_empty);
  props.set_battery_percent(battery_percent);

  return props;
}

// Sets the PowerSupplyProperties on FakePowerManagerClient. Calling this
// method immediately notifies PowerManagerClient observers. One of
// `time_to_full` or `time_to_empty` must be either -1 or a positive number.
// The other must be 0. If `battery_state` is NOT_PRESENT, both `time_to_full`
// and `time_to_empty` will be left unset.
void SetPowerManagerProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty,
    double battery_percent) {
  power_manager::PowerSupplyProperties props = ConstructPowerSupplyProperties(
      power_source, battery_state, is_calculating_battery_time, time_to_full,
      time_to_empty, battery_percent);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
}

// Get the path to file manager's test data directory.
base::FilePath GetTestDataFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");

  // Return full test data path to the given |file_name|.
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

// Copy a file from the file manager's test data directory to the specified
// target_path.
void AddFile(const std::string& file_name,
             int64_t expected_size,
             base::FilePath target_path) {
  const base::FilePath entry_path = GetTestDataFilePath(file_name);
  target_path = target_path.AppendASCII(file_name);
  ASSERT_TRUE(base::CopyFile(entry_path, target_path))
      << "Copy from " << entry_path.value() << " to " << target_path.value()
      << " failed.";
  // Verify file size.
  base::stat_wrapper_t stat;
  const int res = base::File::Lstat(target_path, &stat);
  ASSERT_FALSE(res < 0) << "Couldn't stat" << target_path.value();
  ASSERT_EQ(expected_size, stat.st_size);
}

healthd_mojom::ProbeErrorPtr CreateProbeError(
    healthd_mojom::ErrorType error_type) {
  auto probe_error = healthd_mojom::ProbeError::New();
  probe_error->type = error_type;
  probe_error->msg = "probe error";
  return probe_error;
}

void VerifyProbeErrorBucketCounts(const base::HistogramTester& tester,
                                  const std::string& metric_name,
                                  size_t expected_unknown_error,
                                  size_t expected_parse_error,
                                  size_t expected_service_unavailable,
                                  size_t expected_system_utility_error,
                                  size_t expected_file_read_error) {
  tester.ExpectBucketCount(metric_name, healthd_mojom::ErrorType::kUnknown,
                           expected_unknown_error);
  tester.ExpectBucketCount(metric_name, healthd_mojom::ErrorType::kParseError,
                           expected_parse_error);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kServiceUnavailable,
                           expected_service_unavailable);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kSystemUtilityError,
                           expected_system_utility_error);
  tester.ExpectBucketCount(metric_name,
                           healthd_mojom::ErrorType::kFileReadError,
                           expected_file_read_error);
}

void VerifyBatteryDataErrorBucketCounts(
    const base::HistogramTester& tester,
    size_t expected_no_data_error,
    size_t expected_not_a_number_error,
    size_t expected_expectation_not_met_error) {
  tester.ExpectBucketCount(kBatteryDataError,
                           system_info::BatteryDataError::kNoData,
                           expected_no_data_error);
  tester.ExpectBucketCount(kBatteryDataError,
                           system_info::BatteryDataError::kNotANumber,
                           expected_not_a_number_error);
  tester.ExpectBucketCount(kBatteryDataError,
                           system_info::BatteryDataError::kExpectationNotMet,
                           expected_expectation_not_met_error);
}

}  // namespace

class SystemInfoCardProviderTest : public testing::Test {
 public:
  SystemInfoCardProviderTest() = default;

  ~SystemInfoCardProviderTest() override = default;

 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::cros_healthd::FakeCrosHealthd::Initialize();

    // Initialize fake DBus clients.
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SpacedClient::InitializeFake();

    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);

    // The storage handler requires an instance of ArcServiceManager
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    auto provider = std::make_unique<SystemInfoCardProvider>(profile_.get());
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));

    // Create and register MyFiles directory.
    // By emulating chromeos running, GetMyFilesFolderForProfile will return the
    // profile's temporary location instead of $HOME/Downloads.
    base::test::ScopedRunningOnChromeOS running_on_chromeos;
    const base::FilePath my_files_path =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    CHECK(base::CreateDirectory(my_files_path));
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        my_files_path));

    Wait();
  }

  void TearDown() override {
    provider_ = nullptr;
    search_controller_.reset();
    profile_.reset();
    arc_service_manager_.reset();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::disks::DiskMountManager::Shutdown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    ash::SpacedClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  const SearchProvider::Results& results() {
    return search_controller_->last_results();
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

  content::BrowserTaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  raw_ptr<SystemInfoCardProvider> provider_;
};

TEST_F(SystemInfoCardProviderTest, Version) {
  StartSearch(u"version");
  Wait();
  std::u16string official =
      version_info::IsOfficialBuild() ? u"Official Build" : u"Developer Build";

  std::u16string size = sizeof(void*) == 8 ? u" (64-bit)" : u" (32-bit)";
  std::u16string version =
      u"Version " +
      base::UTF8ToUTF16(version_info::GetVersionStringWithModifier("")) +
      u" (" + official + u") " +
      base::UTF8ToUTF16(
          chrome::GetChannelName(chrome::WithExtendedStable(true))) +
      size;

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  EXPECT_EQ(results()[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(results()[0]->result_type(),
            ash::AppListSearchResultType::kSystemInfo);
  EXPECT_EQ(results()[0]->metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kTextCard);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), version);
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Click to check for details");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, PreventTriggeringOfTooShortQueries) {
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  provider_->SetCpuUsageTimerForTesting(std::move(timer));

  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;
  uint32_t core_1_speed = 4000000;
  uint32_t core_2_speed = 2000000;
  system_info::CpuUsageData core_1(1000, 1000, 1000);
  system_info::CpuUsageData core_2(2000, 2000, 2000);

  SetCrosHealthdCpuResponse({core_1, core_2}, {temp_1, temp_2, temp_3},
                            {core_1_speed, core_2_speed});
  StartSearch(u"cp");
  Wait();
  ASSERT_TRUE(results().empty());

  StartSearch(u"c");
  Wait();
  ASSERT_TRUE(results().empty());

  StartSearch(u"cpu");
  Wait();
  ASSERT_FALSE(results().empty());
}

TEST_F(SystemInfoCardProviderTest, Cpu) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  provider_->SetCpuUsageTimerForTesting(std::move(timer));

  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;
  uint32_t core_1_speed = 4000000;
  uint32_t core_2_speed = 2000000;
  system_info::CpuUsageData core_1(1000, 1000, 1000);
  system_info::CpuUsageData core_2(2000, 2000, 2000);

  SetCrosHealthdCpuResponse({core_1, core_2}, {temp_1, temp_2, temp_3},
                            {core_1_speed, core_2_speed});

  StartSearch(u"cpu");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  EXPECT_EQ(results()[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(results()[0]->result_type(),
            ash::AppListSearchResultType::kSystemInfo);
  EXPECT_EQ(results()[0]->metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kTextCard);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"CPU usage snapshot: 66%");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Temperature: 35°C - Current speed: 3GHz");
  EXPECT_TRUE(details.GetTextTags().empty());

  int new_temp_1 = 20;
  int new_temp_2 = 30;
  int new_temp_3 = 10;
  core_1_speed = 5000000;
  core_2_speed = 6000000;

  system_info::CpuUsageData core_1_delta(3000, 2500, 4500);
  system_info::CpuUsageData core_2_delta(1000, 5500, 3500);

  SetCrosHealthdCpuResponse({core_1 + core_1_delta, core_2 + core_2_delta},
                            {new_temp_1, new_temp_2, new_temp_3},
                            {core_1_speed, core_2_speed});

  timer_ptr->Fire();
  Wait();

  EXPECT_EQ(title.GetText(), u"CPU usage snapshot: 60%");
  EXPECT_EQ(details.GetText(), u"Temperature: 20°C - Current speed: 5.5GHz");

  SetCrosHealthdCpuResponse({core_1 + core_1_delta + core_1_delta,
                             core_2 + core_2_delta + core_2_delta},
                            {new_temp_1, new_temp_2, new_temp_3},
                            {core_1_speed, core_2_speed});

  StartSearch(u"cpu");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  const auto& title2 = results()[0]->title_text_vector()[0];
  EXPECT_EQ(title2.GetText(), u"CPU usage snapshot: 60%");
  const auto& details2 = results()[0]->details_text_vector()[0];
  EXPECT_EQ(details2.GetText(), u"Temperature: 20°C - Current speed: 5.5GHz");
}

TEST_F(SystemInfoCardProviderTest, CpuProbeError) {
  auto info = healthd_mojom::TelemetryInfo::New();
  base::HistogramTester histogram_tester;

  auto cpu_result = healthd_mojom::CpuResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kFileReadError));
  info->cpu_result = std::move(cpu_result);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  StartSearch(u"cpu");
  Wait();

  EXPECT_TRUE(results().empty());
  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorCpuInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/1);
}

TEST_F(SystemInfoCardProviderTest, Memory) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  provider_->SetMemoryTimerForTesting(std::move(timer));

  const uint32_t total_memory_kib = 8000000;
  const uint32_t free_memory_kib = 2000000;
  const uint32_t available_memory_kib = 4000000;

  SetCrosHealthdMemoryUsageResponse(total_memory_kib, free_memory_kib,
                                    available_memory_kib);

  StartSearch(u"memory");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  EXPECT_EQ(results()[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(results()[0]->result_type(),
            ash::AppListSearchResultType::kSystemInfo);
  EXPECT_EQ(results()[0]->metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kBarChart);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            50);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  EXPECT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  EXPECT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Memory 3.8 GB | 7.6 GB total");
  EXPECT_TRUE(details.GetTextTags().empty());

  const uint32_t total_memory_kib_2 = 8000000;
  const uint32_t free_memory_kib_2 = 2000000;
  const uint32_t available_memory_kib_2 = 2000000;

  SetCrosHealthdMemoryUsageResponse(total_memory_kib_2, free_memory_kib_2,
                                    available_memory_kib_2);

  timer_ptr->Fire();
  Wait();

  EXPECT_EQ(title.GetText(), u"");
  EXPECT_EQ(details.GetText(), u"Memory 1.9 GB | 7.6 GB total");
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            75);

  StartSearch(u"memory");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  const auto& details2 = results()[0]->details_text_vector()[0];
  EXPECT_EQ(details2.GetText(), u"Memory 1.9 GB | 7.6 GB total");
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            75);
  EXPECT_EQ(results()[0]
                ->system_info_answer_card_data()
                ->upper_warning_limit_bar_chart.value(),
            90);
}

TEST_F(SystemInfoCardProviderTest, MemoryProbeError) {
  auto info = healthd_mojom::TelemetryInfo::New();
  base::HistogramTester histogram_tester;

  auto memory_result = healthd_mojom::MemoryResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kSystemUtilityError));
  info->memory_result = std::move(memory_result);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  StartSearch(u"memory");
  Wait();

  EXPECT_TRUE(results().empty());
  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorMemoryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/0,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/1,
                               /*expected_file_read_error=*/0);
}

TEST_F(SystemInfoCardProviderTest, Battery) {
  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);
  Wait();

  StartSearch(u"battery");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  EXPECT_EQ(results()[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(results()[0]->result_type(),
            ash::AppListSearchResultType::kSystemInfo);
  EXPECT_EQ(results()[0]->metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kBarChart);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            94);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Battery 94% | 17 minutes until full");
  EXPECT_TRUE(details.GetTextTags().empty());

  EXPECT_EQ(results()[0]->system_info_answer_card_data()->extra_details,
            u"Battery health 76% | Cycle count 500");

  const int64_t new_time_to_full_secs = time_to_full_secs - 100;
  const double new_battery_percent = 96.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, new_time_to_full_secs,
                            time_to_empty_secs, new_battery_percent);
  Wait();

  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            96);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& updated_title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(updated_title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(updated_title.GetText(), u"");
  EXPECT_TRUE(updated_title.GetTextTags().empty());

  const auto& updated_details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(updated_details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(updated_details.GetText(), u"Battery 96% | 15 minutes until full");
  EXPECT_TRUE(updated_details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, BatteryWhileCalculating) {
  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = true;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);
  StartSearch(u"battery");
  Wait();

  EXPECT_EQ(results()[0]->system_info_answer_card_data()->bar_chart_percentage,
            94);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& calculating_title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(calculating_title.GetType(),
            ash::SearchResultTextItemType::kString);
  EXPECT_EQ(calculating_title.GetText(), u"");
  EXPECT_TRUE(calculating_title.GetTextTags().empty());

  const auto& calculating_details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(calculating_details.GetType(),
            ash::SearchResultTextItemType::kString);
  EXPECT_EQ(calculating_details.GetText(), u"Battery 94%");
  EXPECT_TRUE(calculating_details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, BatteryProbeError) {
  auto info = healthd_mojom::TelemetryInfo::New();
  base::HistogramTester histogram_tester;

  auto battery_result = healthd_mojom::BatteryResult::NewError(
      CreateProbeError(healthd_mojom::ErrorType::kParseError));
  info->battery_result = std::move(battery_result);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);

  StartSearch(u"battery");
  Wait();

  VerifyProbeErrorBucketCounts(histogram_tester, kProbeErrorBatteryInfo,
                               /*expected_unknown_error=*/0,
                               /*expected_parse_error=*/1,
                               /*expected_service_unavailable=*/0,
                               /*expected_system_utility_error=*/0,
                               /*expected_file_read_error=*/0);
  EXPECT_TRUE(results().empty());
}

TEST_F(SystemInfoCardProviderTest, BatteryProbeDataError) {
  auto info = healthd_mojom::TelemetryInfo::New();
  base::HistogramTester histogram_tester;

  SetCrosHealthdBatteryHealthResponse(0, 0, 0);

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);

  StartSearch(u"battery");
  Wait();

  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/0,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/1);
  EXPECT_TRUE(results().empty());
}

TEST_F(SystemInfoCardProviderTest, BatteryPowerManagerError) {
  auto info = healthd_mojom::TelemetryInfo::New();
  base::HistogramTester histogram_tester;

  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  std::nullopt_t props = std::nullopt;
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);

  StartSearch(u"battery");
  Wait();

  VerifyBatteryDataErrorBucketCounts(histogram_tester,
                                     /*expected_no_data_error=*/1,
                                     /*expected_not_a_number_error=*/0,
                                     /*expected_expectation_not_met_error=*/0);
  EXPECT_TRUE(results().empty());
}

TEST_F(SystemInfoCardProviderTest, Storage) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Get local filesystem storage statistics.
  const base::FilePath mount_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_.get());

  const base::FilePath android_files_path =
      profile_->GetPath().Append("AndroidFiles");
  const base::FilePath android_files_download_path =
      android_files_path.Append("Download");

  // Create directories.
  CHECK(base::CreateDirectory(downloads_path));
  CHECK(base::CreateDirectory(android_files_path));

  // Register android files mount point.
  CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      android_files_path));

  const int kMountPathBytes = 8092;
  const int kAndroidPathBytes = 15271;
  const int kDownloadsPathBytes = 56758;

  // Add files in MyFiles and Android files.
  AddFile("random.bin", kMountPathBytes, mount_path);          // ~7.9 KB
  AddFile("tall.pdf", kAndroidPathBytes, android_files_path);  // ~14.9 KB
  // Add file in Downloads and simulate bind mount with
  // [android files]/Download.
  AddFile("video.ogv", kDownloadsPathBytes, downloads_path);  // ~55.4 KB

  int64_t total_bytes = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  int64_t available_bytes = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  int64_t rounded_total_size = ash::settings::RoundByteSize(total_bytes);

  int64_t in_use_bytes = rounded_total_size - available_bytes;
  std::u16string in_use_size = ui::FormatBytes(in_use_bytes);
  std::u16string total_size = ui::FormatBytes(rounded_total_size);
  std::u16string result_description = base::StrCat(
      {u"Storage ", in_use_size, u" in use | ", total_size, u" total"});

  StartSearch(u"storage");

  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results().size(), 1u);
  EXPECT_EQ(results()[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(results()[0]->result_type(),
            ash::AppListSearchResultType::kSystemInfo);
  EXPECT_EQ(results()[0]->metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(results()[0]->system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kBarChart);
  auto found_bar_chart_percentage =
      results()[0]->system_info_answer_card_data()->bar_chart_percentage;
  auto expected_bar_chart_percentage = in_use_bytes * 100 / rounded_total_size;
  EXPECT_EQ(expected_bar_chart_percentage, found_bar_chart_percentage);

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), result_description);
  EXPECT_TRUE(details.GetTextTags().empty());
}

}  // namespace app_list::test
