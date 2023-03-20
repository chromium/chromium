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
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/webui/settings/ash/device_storage_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
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
    const std::vector<CpuUsageData>& usage_data,
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

// Sets the CpuUsage response on cros_healthd. `usage_data` should contain one
// entry for each logical cpu.
void SetCrosHealthdCpuUsageResponse(
    const std::vector<CpuUsageData>& usage_data) {
  // Use fake temp and scaled clock speed data since none was supplied.
  const std::vector<uint32_t> scaled_clock_speeds(usage_data.size(), 10000);
  SetCrosHealthdCpuResponse(usage_data, {50}, scaled_clock_speeds);
}

void SetCrosHealthdCpuTemperatureResponse(
    const std::vector<int32_t>& cpu_temps) {
  // Use fake usage_data and scaled clock speed data since none was supplied.
  SetCrosHealthdCpuResponse({CpuUsageData(1000, 1000, 1000)}, cpu_temps,
                            {10000});
}

void SetCrosHealthdCpuScalingResponse(const std::vector<uint32_t>& cpu_speeds) {
  // Use fake temp and usage_data data since none was supplied.
  const std::vector<CpuUsageData> usage_data(cpu_speeds.size(),
                                             CpuUsageData(1000, 1000, 1000));
  SetCrosHealthdCpuResponse(usage_data, {50}, cpu_speeds);
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
      /*manufacture_date=*/absl::nullopt, std::move(temp_value_ptr));
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
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
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
  const int res = base::File::Lstat(target_path.value().c_str(), &stat);
  ASSERT_FALSE(res < 0) << "Couldn't stat" << target_path.value();
  ASSERT_EQ(expected_size, stat.st_size);
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
        new file_manager::FakeDiskMountManager);

    // The storage handler requires an instance of ArcServiceManager
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    provider_ = std::make_unique<SystemInfoCardProvider>(profile_.get());
    provider_->set_controller(search_controller_.get());

    // Create and register My files directory.
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
    provider_.reset();
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

  void StartSearch(const std::u16string& query) { provider_->Start(query); }

  content::BrowserTaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<SystemInfoCardProvider> provider_;
};

TEST_F(SystemInfoCardProviderTest, version) {
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
  EXPECT_EQ(details.GetText(), u"Check for updates");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, cpu) {
  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;
  uint32_t core_1_speed = 4000;
  uint32_t core_2_speed = 5000;
  CpuUsageData core_1(1000, 1000, 1000);
  CpuUsageData core_2(2000, 2000, 2000);

  SetCrosHealthdCpuUsageResponse({core_1, core_2});
  SetCrosHealthdCpuScalingResponse({core_1_speed, core_2_speed});
  SetCrosHealthdCpuTemperatureResponse({temp_1, temp_2, temp_3});

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
  EXPECT_EQ(title.GetText(), u"CPU current usage: 66%");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Temperature: 35°C - Current speed: 0.01GHz");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, memory) {
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
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"3.8 GB of 7.6 GB available");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, battery) {
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
  EXPECT_EQ(title.GetText(), u"94% | 17 minutes until full");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Battery health 76% | Cycle count 500");
  EXPECT_TRUE(details.GetTextTags().empty());

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
  EXPECT_EQ(updated_title.GetText(), u"96% | 15 minutes until full");
  EXPECT_TRUE(updated_title.GetTextTags().empty());
}

TEST_F(SystemInfoCardProviderTest, storage) {
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
  const int kDownloadsPathBytes = 59943;

  // Add files in My files and android files.
  AddFile("random.bin", kMountPathBytes, mount_path);          // ~7.9 KB
  AddFile("tall.pdf", kAndroidPathBytes, android_files_path);  // ~14.9 KB
  // Add file in Downloads and simulate bind mount with
  // [android files]/Download.
  AddFile("video.ogv", kDownloadsPathBytes, downloads_path);  // ~58.6 KB

  int64_t total_bytes = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  int64_t available_bytes = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  int64_t rounded_total_size = ash::settings::RoundByteSize(total_bytes);

  int64_t in_use_bytes = rounded_total_size - available_bytes;
  std::u16string in_use_size = ui::FormatBytes(in_use_bytes);
  std::u16string total_size = ui::FormatBytes(rounded_total_size);
  std::u16string result_title =
      base::StrCat({in_use_size, u" in use / ", total_size});

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
            ash::SystemInfoAnswerCardDisplayType::kMultiElementBarChart);
  auto storage_type_to_size =
      results()[0]->system_info_answer_card_data()->storage_type_to_size;
  EXPECT_EQ(
      ui::FormatBytes(
          storage_type_to_size[ash::SearchResultSystemInfoStorageType::kTotal]),
      total_size);
  EXPECT_EQ(
      ui::FormatBytes(storage_type_to_size
                          [ash::SearchResultSystemInfoStorageType::kMyFiles]),
      ui::FormatBytes(kMountPathBytes + kAndroidPathBytes +
                      kDownloadsPathBytes));

  ASSERT_EQ(results()[0]->title_text_vector().size(), 1u);
  const auto& title = results()[0]->title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), result_title);
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(results()[0]->details_text_vector().size(), 1u);
  const auto& details = results()[0]->details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"");
  EXPECT_TRUE(details.GetTextTags().empty());
}

}  // namespace app_list::test
