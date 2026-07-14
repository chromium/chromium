// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_
#define CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/state_observer.h"

// WARNING: This file is used by internal tests. Updates to the API here may
// cause internal bot build/test failures, please use caution. This file
// re-exports some functionality from elsewhere, just to ensure it isn't updated
// inadvertently.

class Profile;

namespace content {
class WebContents;
class TestDevToolsProtocolClient;
}  // namespace content

using captured_sites_test_utils::WebPageReplayServerWrapper;

namespace glic::test {

// Note: Requires --run-live-tests to run any of the tests.
class GlicE2ETest : public InteractiveBrowserTestMixin<signin::test::LiveTest>,
                    public Host::Observer {
 public:
  explicit GlicE2ETest(const std::vector<base::test::FeatureRef>&
                           additional_enabled_features = {},
                       const std::vector<base::test::FeatureRef>&
                           additional_disabled_features = {});
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

  // Host::Observer implementation:
  void WebUiStateChanged(glic::mojom::WebUiState state) override;

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

  // Sets the pref to enable or disable actuation on web for the user.
  void SetUserEnabledActuationOnWeb(bool enabled);

  // Helper function to clear focus from the omnibox.
  ui::InteractionSequence::StepBuilder ClearOmniboxFocus();

  void ThrottleCurrentTabNetwork();
  void ThrottleWebContentsNetwork(content::WebContents* web_contents);
  void ThrottleGlicNetwork();

  GlicKeyedService* glic_service();
  GlicInstanceCoordinator& instance_coordinator();
  WebPageReplayServerWrapper* web_page_replay_server_wrapper();
  tabs::TabInterface* active_tab();

  GlicE2ETestMode test_mode() const { return test_mode_; }
  bool run_low_bandwidth_tests() { return enable_low_bandwidth_tests_; }
  bool run_actor_tests() const { return running_actor_tests_; }

  void set_test_account_label(const std::string& test_account_label) {
    ASSERT_TRUE(GetTestAccounts()->GetAccount(test_account_label).has_value());
    test_account_label_ = test_account_label;
  }

  // Used in tests that expect Glic loading to fail with one of the error states
  // (error pages) that are monitored by WebUiStateChanged and cause an early
  // failure.
  void set_expects_error(bool expects_error) { expects_error_ = expects_error; }

 protected:
  // Opt-in flag for using WPR for some requests in real_backend mode.
  bool use_wpr_for_real_backend_ = false;

 private:
  void OnActiveInstanceChanged(GlicInstance* new_instance);

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList exempt_actor_policy_control_feature_list_;
  bool enable_low_bandwidth_tests_ = false;
  bool running_actor_tests_ = false;
  GlicE2ETestMode test_mode_;
  std::unique_ptr<WebPageReplayServerWrapper> web_page_replay_server_wrapper_;
  std::map<content::WebContents*,
           std::unique_ptr<content::TestDevToolsProtocolClient>>
      devtools_clients_;
  std::string test_account_label_;

  bool expects_error_ = false;
  base::CallbackListSubscription active_instance_subscription_;
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};
  base::WeakPtrFactory<GlicE2ETest> weak_ptr_factory_{this};
};

// Observes Actor task state changes.
class GlicActorTaskState
    : public ui::test::StateObserver<actor::ActorTask::State> {
 public:
  using State = actor::ActorTask::State;

  explicit GlicActorTaskState(Profile* profile);
  ~GlicActorTaskState() override;

 private:
  void StateChanged(actor::ActorTask& task);

  actor::TaskId task_id_;
  base::CallbackListSubscription actor_task_listener_;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicActorTaskState, kGlicActorTaskState);

extern const ui::ElementIdentifier kGlicHandoffButtonElementId;

const base::Feature& GetGlicActionAllowlistFeature();
const char* GetDisableActorSafetyChecksSwitch();
const base::Feature& GetGlicLiveModeFeature();
const base::Feature& GetGlicMultiInstanceFeature();
ui::ElementIdentifier GetGlicButtonElementId();
ui::ElementIdentifier GetTabStripElementId();
ui::ElementIdentifier GetOmniboxElementId();
ui::ElementIdentifier GetGlicViewElementId();

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_E2E_TEST_GLIC_E2E_TEST_H_
