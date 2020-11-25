// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cloud_print_private/cloud_print_private_api.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/cloud_print_private.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Property;
using ::testing::Return;
using ::testing::_;

// A base class for tests below.
class ExtensionCloudPrintPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kCloudPrintURL,
        "http://www.cloudprintapp.com/extensions/api_test/"
        "cloud_print_private");
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.cloudprintapp.com", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  // Returns a test server URL, but with host 'www.cloudprintapp.com' so it
  // matches the cloud print app's extent that we set up via command line flags.
  GURL GetTestServerURL(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(
        "/extensions/api_test/cloud_print_private/" + path);

    // Replace the host with 'www.cloudprintapp.com' so it matches the cloud
    // print app's extent.
    GURL::Replacements replace_host;
    replace_host.SetHostStr("www.cloudprintapp.com");
    return url.ReplaceComponents(replace_host);
  }
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)

using extensions::api::cloud_print_private::UserSettings;

class CloudPrintTestsDelegateMock : public extensions::CloudPrintTestsDelegate {
 public:
  CloudPrintTestsDelegateMock() {}

  MOCK_METHOD4(SetupConnector,
               void(const std::string& user_email,
                    const std::string& robot_email,
                    const std::string& credentials,
                    const UserSettings& user_settings));
  MOCK_METHOD0(GetHostName, std::string());
  MOCK_METHOD0(GetPrinters, std::vector<std::string>());
  MOCK_METHOD0(GetClientId, std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintTestsDelegateMock);
};

MATCHER(IsExpectedUserSettings, "") {
  const UserSettings& settings = arg;
  return settings.connect_new_printers && settings.printers.size() == 2 &&
         settings.printers[0].name == "printer1" &&
         !settings.printers[0].connect &&
         settings.printers[1].name == "printer2" &&
         settings.printers[1].connect;
}

IN_PROC_BROWSER_TEST_F(ExtensionCloudPrintPrivateApiTest,
                       CloudPrintHostedWithMock) {
  CloudPrintTestsDelegateMock cloud_print_mock;

  EXPECT_CALL(cloud_print_mock,
              SetupConnector("foo@gmail.com",
                             "foorobot@googleusercontent.com",
                             "1/23546efa54",
                             IsExpectedUserSettings()));
  EXPECT_CALL(cloud_print_mock, GetHostName())
      .WillRepeatedly(Return("TestHostName"));

  std::vector<std::string> printers;
  printers.push_back("printer1");
  printers.push_back("printer2");
  EXPECT_CALL(cloud_print_mock, GetPrinters())
      .WillRepeatedly(Return(printers));

  EXPECT_CALL(cloud_print_mock, GetClientId())
      .WillRepeatedly(Return("TestAPIClient"));

  // Run this as a hosted app. Since we have overridden the cloud print service
  // URL in the command line, this URL should match the web extent for our
  // cloud print component app and it should work.
  GURL page_url = GetTestServerURL(
      "enable_chrome_connector/cloud_print_success_tests.html");
  ASSERT_TRUE(RunPageTest(page_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCloudPrintPrivateApiTest,
                       CloudPrintHostedIncognito) {
  GURL page_url = GetTestServerURL(
      "enable_chrome_connector/cloud_print_incognito_failure_tests.html");
  ASSERT_TRUE(RunPageTest(page_url.spec(), kFlagNone, kFlagUseIncognito));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
