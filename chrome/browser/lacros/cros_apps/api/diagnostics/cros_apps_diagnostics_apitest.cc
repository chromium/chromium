// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/system/sys_info.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"
#include "chrome/browser/chromeos/cros_apps/api/test/cros_apps_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/cpp/telemetry/fake_probe_service.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrosAppsDiagnosticsApiTest : public CrosAppsApiTest {
 public:
  CrosAppsDiagnosticsApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kBlinkExtensionDiagnostics);
  }

  void SetUpOnMainThread() override {
    CrosAppsApiTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    CrosAppsApiMutableRegistry::GetInstance(browser()->profile())
        .AddOrReplaceForTesting(std::move(
            CrosAppsApiInfo(
                blink::mojom::RuntimeFeature::kBlinkExtensionDiagnostics,
                &blink::RuntimeFeatureStateContext::
                    SetBlinkExtensionDiagnosticsEnabled)
                .AddAllowlistedOrigins({embedded_test_server()->GetOrigin()})));

    ASSERT_TRUE(
        NavigateToURL(browser()->tab_strip_model()->GetActiveWebContents(),
                      embedded_test_server()->GetURL("/empty.html")));
  }

 protected:
  void SetProbeServiceForTesting(
      std::unique_ptr<chromeos::FakeProbeService> service) {
    fake_probe_service_ = std::move(service);
    // Replace the production probe service with a mock one for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_probe_service_->BindNewPipeAndPassRemote());
  }
  std::unique_ptr<chromeos::FakeProbeService> fake_probe_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrosAppsDiagnosticsApiTest, DiagnosticsExists) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      "typeof window.chromeos.diagnostics !== 'undefined';"));
}

IN_PROC_BROWSER_TEST_F(CrosAppsDiagnosticsApiTest, GetCpuInfo_Success) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Under normal circumstances, the returned values won't be as large as in the
  // following test cases; we use big values here to test edge cases.
  struct TestLogicalCpuInfo {
    uint32_t core_id;
    uint64_t idle_time_ms;
    uint32_t max_clock_speed_khz;
    uint32_t scaling_current_frequency_khz;
    uint32_t scaling_max_frequency_khz;
  } kTestLogicalCpus[] = {
      {
          .core_id = 42,
          .idle_time_ms = 9007199254740991,
          .max_clock_speed_khz = 4294967295,
          .scaling_current_frequency_khz = 536904245,
          .scaling_max_frequency_khz = 1073764046,
      },
      {
          .core_id = 43,
          .idle_time_ms = 9007199254740891,
          .max_clock_speed_khz = 1147494759,
          .scaling_current_frequency_khz = 936904246,
          .scaling_max_frequency_khz = 1063764047,
      },
      {
          .core_id = 44,
          .idle_time_ms = 9007199254740791,
          .max_clock_speed_khz = 1247494759,
          .scaling_current_frequency_khz = 946904246,
          .scaling_max_frequency_khz = 1263764048,
      },
  };

  const crosapi::mojom::ProbeCpuArchitectureEnum kTestCpuArchitecture =
      crosapi::mojom::ProbeCpuArchitectureEnum::kX86_64;
  const std::string kTestCpuArchitectureName = "x86_64";
  const std::string kTestCpuModelName = "AMD Ryzen 7 7840U";

  // Some telemetry info is not available in browser tests, so we need to
  // set up a fake probe service for testing.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    {
      auto logical_cpu1 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_cpu1->core_id =
          crosapi::mojom::UInt32Value::New(kTestLogicalCpus[0].core_id);
      logical_cpu1->idle_time_ms =
          crosapi::mojom::UInt64Value::New(kTestLogicalCpus[0].idle_time_ms);
      logical_cpu1->max_clock_speed_khz = crosapi::mojom::UInt32Value::New(
          kTestLogicalCpus[0].max_clock_speed_khz);
      logical_cpu1->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[0].scaling_current_frequency_khz);
      logical_cpu1->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[0].scaling_max_frequency_khz);

      auto logical_cpu2 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_cpu2->core_id =
          crosapi::mojom::UInt32Value::New(kTestLogicalCpus[1].core_id);
      logical_cpu2->idle_time_ms =
          crosapi::mojom::UInt64Value::New(kTestLogicalCpus[1].idle_time_ms);
      logical_cpu2->max_clock_speed_khz = crosapi::mojom::UInt32Value::New(
          kTestLogicalCpus[1].max_clock_speed_khz);
      logical_cpu2->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[1].scaling_current_frequency_khz);
      logical_cpu2->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[1].scaling_max_frequency_khz);

      auto physical_cpu1 = crosapi::mojom::ProbePhysicalCpuInfo::New();
      physical_cpu1->model_name = kTestCpuModelName;
      physical_cpu1->logical_cpus.push_back(std::move(logical_cpu1));
      physical_cpu1->logical_cpus.push_back(std::move(logical_cpu2));

      auto logical_cpu3 = crosapi::mojom::ProbeLogicalCpuInfo::New();
      logical_cpu3->core_id =
          crosapi::mojom::UInt32Value::New(kTestLogicalCpus[2].core_id);
      logical_cpu3->idle_time_ms =
          crosapi::mojom::UInt64Value::New(kTestLogicalCpus[2].idle_time_ms);
      logical_cpu3->max_clock_speed_khz = crosapi::mojom::UInt32Value::New(
          kTestLogicalCpus[2].max_clock_speed_khz);
      logical_cpu3->scaling_current_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[2].scaling_current_frequency_khz);
      logical_cpu3->scaling_max_frequency_khz =
          crosapi::mojom::UInt32Value::New(
              kTestLogicalCpus[2].scaling_max_frequency_khz);

      auto physical_cpu2 = crosapi::mojom::ProbePhysicalCpuInfo::New();
      physical_cpu2->model_name = kTestCpuModelName;
      physical_cpu2->logical_cpus.push_back(std::move(logical_cpu3));

      auto cpu_info = crosapi::mojom::ProbeCpuInfo::New();
      cpu_info->num_total_threads =
          crosapi::mojom::UInt32Value::New(2147483647);
      cpu_info->architecture = kTestCpuArchitecture;
      cpu_info->physical_cpus.push_back(std::move(physical_cpu1));
      cpu_info->physical_cpus.push_back(std::move(physical_cpu2));

      telemetry_info->cpu_result =
          crosapi::mojom::ProbeCpuResult::NewCpuInfo(std::move(cpu_info));
    }
    auto service = std::make_unique<chromeos::FakeProbeService>();
    service->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    SetProbeServiceForTesting(std::move(service));
  }

  {
    base::Value::List logical_cpus_expected_list;
    for (const auto& logical_cpu : kTestLogicalCpus) {
      // `base::Value::Dict().Set()` expects int type; to avoid type casting
      // issues, we compare properties using strings instead.
      logical_cpus_expected_list.Append(
          base::Value::Dict()
              .Set("coreId", base::NumberToString(logical_cpu.core_id))
              .Set("idleTimeMs", base::NumberToString(logical_cpu.idle_time_ms))
              .Set("maxClockSpeedKhz",
                   base::NumberToString(logical_cpu.max_clock_speed_khz))
              .Set("scalingCurrentFrequencyKhz",
                   base::NumberToString(
                       logical_cpu.scaling_current_frequency_khz))
              .Set(
                  "scalingMaxFrequencyKhz",
                  base::NumberToString(logical_cpu.scaling_max_frequency_khz)));
    }
    base::Value logical_cpus_expected(std::move(logical_cpus_expected_list));

    EXPECT_TRUE(
        content::ExecJs(web_contents,
                        "(async () => { window.cpuInfoResult = await "
                        "window.chromeos.diagnostics.getCpuInfo(); })();"));

    EXPECT_EQ(kTestCpuArchitectureName,
              content::EvalJs(web_contents,
                              "window.cpuInfoResult.architectureName;"));
    EXPECT_EQ(kTestCpuModelName,
              content::EvalJs(web_contents, "window.cpuInfoResult.modelName;"));
    ;

    // JavaScript function that converts all properties of an object to
    // string recursively.
    std::string kDefineToStringFunctionScript =
        R"(function toString(obj) {
            Object.keys(obj).forEach(key => {
                if (typeof obj[key] === 'object') {
                    return toString(obj[key]);
                }
                obj[key] = obj[key].toString();
            });
            return obj;
        })";

    EXPECT_EQ(
        logical_cpus_expected,
        content::EvalJs(web_contents,
                        kDefineToStringFunctionScript +
                            "toString(window.cpuInfoResult.logicalCpus);"));
    EXPECT_THAT(fake_probe_service_->GetLastRequestedCategories(),
                testing::UnorderedElementsAreArray(
                    {crosapi::mojom::ProbeCategoryEnum::kCpu}));
  }
}

IN_PROC_BROWSER_TEST_F(CrosAppsDiagnosticsApiTest,
                       GetCpuInfo_Error_TelemetryProbeServiceUnavailable) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Intentionally skip setting up fake probe service to test the error case.

  EXPECT_EQ(
      "a JavaScript error: \"TelemetryProbeService is unavailable.\"\n",
      content::EvalJs(web_contents, "window.chromeos.diagnostics.getCpuInfo();")
          .error);
}

IN_PROC_BROWSER_TEST_F(CrosAppsDiagnosticsApiTest,
                       GetCpuInfo_Error_CpuTelemetryInfoUnavailable) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Some telemetry info is not available in browser tests, so we need to
  // set up a fake probe service for testing.
  {
    auto telemetry_info = crosapi::mojom::ProbeTelemetryInfo::New();
    auto error = crosapi::mojom::ProbeError::New();
    error->type = crosapi::mojom::ProbeErrorType::kUnknown;
    error->msg = "Unknown error.";
    telemetry_info->cpu_result =
        crosapi::mojom::ProbeCpuResult::NewError(std::move(error));

    auto service = std::make_unique<chromeos::FakeProbeService>();
    service->SetProbeTelemetryInfoResponse(std::move(telemetry_info));
    SetProbeServiceForTesting(std::move(service));
  }

  EXPECT_EQ(
      "a JavaScript error: \"TelemetryProbeService returned an error when "
      "retrieving CPU telemetry info.\"\n",
      content::EvalJs(web_contents, "window.chromeos.diagnostics.getCpuInfo();")
          .error);
  EXPECT_THAT(fake_probe_service_->GetLastRequestedCategories(),
              testing::UnorderedElementsAreArray(
                  {crosapi::mojom::ProbeCategoryEnum::kCpu}));
}

IN_PROC_BROWSER_TEST_F(CrosAppsDiagnosticsApiTest, GetNetworkInterfaces) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<const std::optional<net::NetworkInterfaceList>&>
      future;
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, future.GetCallback());
  std::optional<net::NetworkInterfaceList> interface_list = future.Get();

  if (!interface_list.has_value()) {
    EXPECT_EQ(
        "a JavaScript error: \"Network interface lookup failed or "
        "unsupported.\"\n",
        content::EvalJs(web_contents,
                        "window.chromeos.diagnostics.getNetworkInterfaces();")
            .error);
    return;
  }

  base::Value::List network_interfaces_expected_list;
  for (const auto& interface : interface_list.value()) {
    network_interfaces_expected_list.Append(
        base::Value::Dict()
            .Set("address", interface.address.ToString())
            .Set("name", interface.name)
            .Set("prefixLength", (int)interface.prefix_length));
  }
  base::Value network_interfaces_expected(
      std::move(network_interfaces_expected_list));

  EXPECT_EQ(
      network_interfaces_expected,
      content::EvalJs(web_contents,
                      "window.chromeos.diagnostics.getNetworkInterfaces();"));
}
