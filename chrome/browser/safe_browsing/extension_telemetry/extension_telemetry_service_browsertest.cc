// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr const char kExtensionName[] = "Simple Test Extension";
constexpr const char kExtensionVersion[] = "0.0.1";
constexpr const char kExtensionContactedHost[] = "example.com";

}  // namespace

using RemoteHostContactedInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo;
using RemoteHostInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;

class ExtensionTelemetryServiceBrowserTest
    : public extensions::ExtensionApiTest {
 public:
  ExtensionTelemetryServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {kExtensionTelemetry, kExtensionTelemetryReportContactedHosts}, {});
    CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_dir_));
    test_extension_dir_ =
        test_extension_dir_.AppendASCII("safe_browsing/extension_telemetry");
  }
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  bool IsTelemetryServiceEnabled(ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->enabled() &&
           !telemetry_service->signal_processors_.empty() &&
           telemetry_service->timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty(ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->extension_store_.empty();
  }

  using ExtensionInfo = ExtensionTelemetryReportRequest_ExtensionInfo;
  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      ExtensionTelemetryService* telemetry_service,
      const extensions::ExtensionId& extension_id) {
    auto iter = telemetry_service->extension_store_.find(extension_id);
    if (iter == telemetry_service->extension_store_.end())
      return nullptr;
    return iter->second.get();
  }

  using TelemetryReport = ExtensionTelemetryReportRequest;
  std::unique_ptr<TelemetryReport> GetTelemetryReport(
      ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->CreateReport();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FilePath test_extension_dir_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsRemoteHostContactedSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher result_catcher;
  // Load extension from the telemetry extension directory.
  const auto* extension =
      LoadExtension(test_extension_dir_.AppendASCII("basic_crx"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());
  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  // Successfully retrieve the extension telemetry instance.
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));
  // Process signal.
  {
    // Verify that the registered extension information is saved in the
    // telemetry service's extension store.
    const ExtensionInfo* info =
        GetExtensionInfoFromExtensionStore(telemetry_service, extension->id());
    EXPECT_EQ(extension->name(), kExtensionName);
    EXPECT_EQ(extension->id(), extension->id());
    EXPECT_EQ(info->version(), kExtensionVersion);
  }
  // Generate telemetry report and verify.
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb =
        GetTelemetryReport(telemetry_service);
    ASSERT_NE(telemetry_report_pb, nullptr);
    // Retrieve the telemetry report for the signal extension.
    int report_index = -1;
    for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
      if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
        report_index = i;
      }
    }
    EXPECT_NE(report_index, -1);
    const auto& extension_report = telemetry_report_pb->reports(report_index);
    EXPECT_EQ(extension_report.extension().id(), extension->id());
    EXPECT_EQ(extension_report.extension().name(), kExtensionName);
    EXPECT_EQ(extension_report.extension().version(), kExtensionVersion);
    // Verify the designated signal extension's report has signal data.
    ASSERT_EQ(extension_report.signals().size(), 1);
    // Verify that extension store has been cleared after creating a telemetry
    // report.
    EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));
    // Verify signal proto from the reports.
    const ExtensionTelemetryReportRequest_SignalInfo& signal =
        extension_report.signals()[0];
    const RemoteHostContactedInfo& remote_host_contacted_info =
        signal.remote_host_contacted_info();
    ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 1);
    const RemoteHostInfo& remote_host_info =
        remote_host_contacted_info.remote_host(0);
    EXPECT_EQ(remote_host_info.contact_count(), static_cast<uint32_t>(1));
    EXPECT_EQ(remote_host_info.url(), kExtensionContactedHost);
  }
}

}  // namespace safe_browsing
