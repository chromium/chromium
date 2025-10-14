// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_
#define CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"

namespace content {
class WebContents;
class TestDevToolsProtocolClient;
}  // namespace content

using captured_sites_test_utils::WebPageReplayServerWrapper;

namespace glic::test {

// Note: Requires --run-live-tests to run any of the tests.
class GlicE2ETest : public InteractiveBrowserTestMixin<signin::test::LiveTest> {
 public:
  GlicE2ETest();
  ~GlicE2ETest() override;

  enum GlicE2ETestMode {
    // Tests connecting to a real web backend.
    kRealBackend,
    // WPR record mode.
    kRecord,
    // WPR replay mode.
    kReplay
  };

  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  void TearDownOnMainThread() override;

  void PreRunTestOnMainThread() override;

  MultiStep WaitForAndInstrumentFre();

  MultiStep WaitForAndInstrumentGlic();

  // Based on the test mode, do a UI signin flow (live mode or record mode),
  // or force signin a fake account (replay mode).
  void LoginTestAccountOrForceFakeSignin();

  // Based on test mode, starts WPR in either record or replay mode, or no-op
  // in real backend test mode.
  void MaybeStartWebPageReplayForRecordingPath(const std::string filename);

  // Sets FRE status as completed.
  void SetFRECompletion();

  void ThrottleCurrentTabNetwork();
  void ThrottleWebContentsNetwork(content::WebContents* web_contents);
  void ThrottleGlicNetwork();

  GlicKeyedService* glic_service();
  GlicWindowController& window_controller();
  GlicFreController& fre_controller();
  WebPageReplayServerWrapper* web_page_replay_server_wrapper();

  GlicE2ETestMode test_mode() const { return test_mode_; }
  bool run_low_bandwidth_tests() { return enable_low_bandwidth_tests_; }
  bool run_actor_tests() const { return running_actor_tests_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool enable_low_bandwidth_tests_ = false;
  bool running_actor_tests_ = false;
  GlicE2ETestMode test_mode_;
  std::unique_ptr<WebPageReplayServerWrapper> web_page_replay_server_wrapper_;
  std::map<content::WebContents*,
           std::unique_ptr<content::TestDevToolsProtocolClient>>
      devtools_clients_;
};

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_
