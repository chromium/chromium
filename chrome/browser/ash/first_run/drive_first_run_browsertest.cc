// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/first_run/drive_first_run_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {

namespace {

// Directory containing data files for the tests.
const char kTestDirectory[] = "drive_first_run";

// Directory containing correct hosted app page served by the test server.
const char kGoodServerDirectory[] = "good";

// Directory containing incorrect hosted app page served by the test server.
const char kBadServerDirectory[] = "bad";

// Name of the test hosted app .crx file.
const char kTestAppCrxName[] = "app.crx";

// App id of the test hosted app.
const char kTestAppId[] = "kipccbklifbfblhpplnmklieangbjnhb";

// The endpoint belonging to the test hosted app.
const char kTestEndpointUrl[] = "http://example.com/endpoint.html";

}  // namespace

class DriveFirstRunTest : public InProcessBrowserTest,
                          public DriveFirstRunController::Observer {
 protected:
  DriveFirstRunTest();

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // DriveFirstRunController::Observer overrides:
  void OnCompletion(bool success) override;
  void OnTimedOut() override;

  void InstallApp();

  void InitTestServer(const std::string& directory);

  bool WaitForFirstRunResult();

  void EnableOfflineMode();

  void SetDelays(int initial_delay_secs, int timeout_secs);

  bool timed_out() const { return timed_out_; }

 private:
  // |controller_| is responsible for its own lifetime.
  DriveFirstRunController* controller_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  bool timed_out_;
  bool waiting_for_result_;
  bool success_;
  base::FilePath test_data_dir_;
  std::string endpoint_url_;
};

DriveFirstRunTest::DriveFirstRunTest() :
    timed_out_(false),
    waiting_for_result_(false),
    success_(false) {}

void DriveFirstRunTest::SetUpOnMainThread() {
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII(kTestDirectory);

  host_resolver()->AddRule("example.com", "127.0.0.1");

  // |controller_| will delete itself when it completes.
  controller_ = new DriveFirstRunController(browser()->profile());
  controller_->AddObserver(this);
  controller_->SetDelaysForTest(0, 10);
  controller_->SetAppInfoForTest(kTestAppId, kTestEndpointUrl);
}

void DriveFirstRunTest::TearDownOnMainThread() {
  content::RunAllPendingInMessageLoop();
}

void DriveFirstRunTest::InitTestServer(const std::string& directory) {
  embedded_test_server()->ServeFilesFromDirectory(
      test_data_dir_.AppendASCII(directory));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Configure the endpoint to use the test server's port.
  const GURL url(kTestEndpointUrl);
  GURL::Replacements replacements;
  std::string port(base::NumberToString(embedded_test_server()->port()));
  replacements.SetPortStr(port);
  endpoint_url_ = url.ReplaceComponents(replacements).spec();
  controller_->SetAppInfoForTest(kTestAppId, endpoint_url_);
}

void DriveFirstRunTest::InstallApp() {
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  ASSERT_TRUE(
      loader.LoadExtension(test_data_dir_.AppendASCII(kTestAppCrxName)));
}

void DriveFirstRunTest::EnableOfflineMode() {
  controller_->EnableOfflineMode();
}

void DriveFirstRunTest::SetDelays(int initial_delay_secs, int timeout_secs) {
  controller_->SetDelaysForTest(initial_delay_secs, timeout_secs);
}

bool DriveFirstRunTest::WaitForFirstRunResult() {
  waiting_for_result_ = true;
  runner_ = new content::MessageLoopRunner;
  runner_->Run();
  EXPECT_FALSE(waiting_for_result_);
  return success_;
}

void DriveFirstRunTest::OnCompletion(bool success) {
  EXPECT_TRUE(waiting_for_result_);
  waiting_for_result_ = false;
  success_ = success;
  runner_->Quit();

  // |controller_| will eventually delete itself upon completion, so invalidate
  // the pointer.
  controller_ = NULL;
}

void DriveFirstRunTest::OnTimedOut() {
  timed_out_ = true;
}

IN_PROC_BROWSER_TEST_F(DriveFirstRunTest, OfflineEnabled) {
  InstallApp();
  InitTestServer(kGoodServerDirectory);
  EnableOfflineMode();
  EXPECT_TRUE(WaitForFirstRunResult());
}

IN_PROC_BROWSER_TEST_F(DriveFirstRunTest, AppNotInstalled) {
  InitTestServer(kGoodServerDirectory);
  EnableOfflineMode();
  EXPECT_FALSE(WaitForFirstRunResult());
  EXPECT_FALSE(timed_out());
}

IN_PROC_BROWSER_TEST_F(DriveFirstRunTest, TimedOut) {
  // Test that the controller times out instead of hanging forever.
  InstallApp();
  InitTestServer(kBadServerDirectory);
  SetDelays(0, 0);
  EnableOfflineMode();
  EXPECT_FALSE(WaitForFirstRunResult());
  EXPECT_TRUE(timed_out());
}

}  // namespace ash
