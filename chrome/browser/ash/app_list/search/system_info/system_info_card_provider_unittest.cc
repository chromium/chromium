// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

class SystemInfoCardProviderTest : public testing::Test {
 public:
  SystemInfoCardProviderTest() = default;

  ~SystemInfoCardProviderTest() override = default;

 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::cros_healthd::FakeCrosHealthd::Initialize();

    // The storage handler requires an instance of ArcServiceManager
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    provider_ = std::make_unique<SystemInfoCardProvider>(profile_.get());
    provider_->set_controller(search_controller_.get());

    Wait();
  }

  void TearDown() override {
    provider_.reset();
    search_controller_.reset();
    profile_.reset();
    arc_service_manager_.reset();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
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

}  // namespace app_list::test
