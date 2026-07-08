// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/e2e_test/glic_e2e_test.h"

#include <map>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/save_desktop_snapshot.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

#if ENABLE_GLIC_INTERNAL_TESTS
#include "chrome/browser/glic/e2e_test/internal/constants.h"
#else
#include "chrome/browser/glic/e2e_test/internal_test_placeholder_constants.h"  // nogncheck
#endif

namespace glic::test {

namespace {

using glic::test::internal::kGlicInstanceCoordinatorState;

constexpr base::FilePath::StringViewType kRecordingDirectoryPath =
    FILE_PATH_LITERAL("chrome/browser/glic/e2e_test/internal/wpr_recordings");

const char kGlicE2ETestModeSwitch[] = "glic-e2e-test-mode";
const char kHostResolverRulesValue[] =
    "MAP *:80 127.0.0.1:8080,MAP *:443 127.0.0.1:8081,EXCLUDE localhost";
constexpr char kEnableActorTests[] = "enable-actor-tests";
const char kEnableLowBandwidthTestsSwitch[] = "enable-low-bandwidth-tests";

// The first 2 is from WPR code readme. The last one is from
// |kWebPageReplayCertSPKI| in
// //chrome/browser/autofill/captured_sites_test_utils.cc
// TODO(crbug.com/399665693): Consolidate the wpr RSA certs in wpr source code
// and used in the C++ test utilities.
const char kIgnoreCertificateErrorsSPKIListValue[] =
    "PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,"
    "2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=,"
    "PoNnQAwghMiLUPg1YNFtvTfGreNT8r9oeLEyzgNCJWc=";
}  // namespace

GlicE2ETest::GlicE2ETest(
    const std::vector<base::test::FeatureRef>& additional_enabled_features,
    const std::vector<base::test::FeatureRef>& additional_disabled_features) {
  std::vector<base::test::FeatureRef> enabled = {
      features::kGlic, features::kGlicKeyboardShortcutNewBadge,
      features::kGlicRollout, kContextualCueing};
  enabled.insert(enabled.end(), additional_enabled_features.begin(),
                 additional_enabled_features.end());

  std::vector<base::test::FeatureRef> disabled = {
      syncer::kReplaceSyncPromosWithSignInPromos,
      syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
      // Don't disable glic based on country/locale.
      features::kGlicCountryFiltering,
      features::kGlicLocaleFiltering,
  };
  disabled.insert(disabled.end(), additional_disabled_features.begin(),
                  additional_disabled_features.end());

  scoped_feature_list_.InitWithFeatures(enabled, disabled);
}

GlicE2ETest::~GlicE2ETest() = default;

void GlicE2ETest::SetUp() {
  const base::CommandLine* command_line_of_test =
      base::CommandLine::ForCurrentProcess();

  std::string test_mode_value =
      command_line_of_test->GetSwitchValueASCII(kGlicE2ETestModeSwitch);

  running_actor_tests_ = command_line_of_test->HasSwitch(kEnableActorTests);
  if (running_actor_tests_) {
    exempt_actor_policy_control_feature_list_
        .InitAndEnableFeatureWithParameters(
            features::kGlicActor,
            {{features::kGlicActorPolicyControlExemption.name, "true"}});
  }
  enable_low_bandwidth_tests_ =
      command_line_of_test->HasSwitch(kEnableLowBandwidthTestsSwitch);

  if (test_mode_value.empty() || test_mode_value == "real_backend") {
    test_mode_ = kRealBackend;
  } else if (test_mode_value == "record") {
    test_mode_ = kRecord;
  } else if (test_mode_value == "replay") {
    test_mode_ = kReplay;
  } else {
    FAIL() << "Incorrect test mode input: %s" << test_mode_value;
  }

  // Initialize WPR if we are in record/replay mode, or if opted-in to use WPR
  // for some requests in real_backend mode.
  if (test_mode_ == kRecord || test_mode_ == kReplay ||
      (test_mode_ == kRealBackend && use_wpr_for_real_backend_)) {
    web_page_replay_server_wrapper_ =
        std::make_unique<WebPageReplayServerWrapper>(
            test_mode_ == kReplay || test_mode_ == kRealBackend, 8080, 8081,
            kWprArguments);
  }

  // Always disable animation for stability.
  gfx::ScopedAnimationDurationScaleMode disable_animation(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  LiveTest::SetUp();
}

void GlicE2ETest::SetUpCommandLine(base::CommandLine* command_line) {
  LiveTest::SetUpCommandLine(command_line);

  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    // The following arguments make browser work with WPR proxy.
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    kHostResolverRulesValue);
  }

  if (test_mode_ == kRecord || test_mode_ == kReplay ||
      (test_mode_ == kRealBackend && use_wpr_for_real_backend_)) {
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList,
        kIgnoreCertificateErrorsSPKIListValue);
  }
}

void GlicE2ETest::PreRunTestOnMainThread() {
  LiveTest::PreRunTestOnMainThread();

  active_instance_subscription_ =
      instance_coordinator()
          .AddActiveInstanceChangedCallbackAndNotifyImmediately(
              base::BindRepeating(&GlicE2ETest::OnActiveInstanceChanged,
                                  base::Unretained(this)));

  GURL glic_guest_url = glic::GetGuestURL();
  CHECK(glic_guest_url.is_valid())
      << "Incorrect GLiC guest URL in cmd line arguments.";

  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    // When WPR is used, for consistency, require consistent host and path.
    CHECK(glic_guest_url.spec().contains(kAllowedHostAndPathForWpr))
        << "Please use allowed URL for WPR.";
  }
}

void GlicE2ETest::LoginTestAccountOrForceFakeSignin() {
  if (test_mode_ == kRealBackend || test_mode_ == kRecord) {
    std::string account_label = test_account_label_;
    // TODO(crbug.com/476984789): Remove this fallback once all tests have been
    // updated to call set_test_account_label().
    if (account_label.empty()) {
      account_label =
          running_actor_tests_ ? kTestActorAccountLabel : kTestAccountLabel;
    }
    std::optional<signin::TestAccountSigninCredentials> test_account =
        GetTestAccounts()->GetAccount(account_label);
    signin::test::SignInFunctions sign_in_functions =
        signin::test::SignInFunctions(
            base::BindLambdaForTesting(
                [this]() -> Browser* { return this->browser(); }),
            base::BindLambdaForTesting(
                [this](int index, const GURL& url,
                       ui::PageTransition transition) -> bool {
                  return this->AddTabAtIndex(index, url, transition);
                }));
    // Sign in to opted in test account.
    CHECK(test_account.has_value());
    sign_in_functions.TurnOnSync(*test_account, 0);
  } else {
    SigninWithPrimaryAccount(browser()->GetProfile());
    SetGlicCapability(browser()->GetProfile(), true);
  }
}

void GlicE2ETest::SetFRECompletion() {
  ::glic::SetFRECompletion(browser()->GetProfile(),
                           prefs::FreStatus::kCompleted);
}

void GlicE2ETest::SetUserEnabledActuationOnWeb(bool enabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicUserEnabledActuationOnWeb, enabled);
}

ui::InteractionSequence::StepBuilder GlicE2ETest::ClearOmniboxFocus() {
  return WithView(kOmniboxElementId, [](OmniboxViewViews* omnibox_view) {
    omnibox_view->GetFocusManager()->ClearFocus();
  });
}

void GlicE2ETest::SetUpInProcessBrowserTestFixture() {
  // Allowlists hosts.
  host_resolver()->AllowDirectLookup("*.google.com");

  LiveTest::SetUpInProcessBrowserTestFixture();
}

void GlicE2ETest::TearDownOnMainThread() {
  if (HasFailure()) {
    base::FilePath snapshot_path = SaveDesktopSnapshot();
    if (!snapshot_path.empty()) {
      LOG(WARNING) << "Saved desktop snapshot to: " << snapshot_path;
    }
  }
  for (auto& client : devtools_clients_) {
    client.second->DetachProtocolClient();
  }
  devtools_clients_.clear();
  if (test_mode_ == kRecord || test_mode_ == kReplay ||
      (test_mode_ == kRealBackend && use_wpr_for_real_backend_)) {
    // Ensure enough time for WPR to write archive at recording mode
    // by putting this in main thread.
    EXPECT_TRUE(web_page_replay_server_wrapper_->Stop())
        << "Cannot stop the local Web Page Replay server.";
  }
  LiveTest::TearDownOnMainThread();
}

ui::test::InteractiveTestApi::MultiStep
GlicE2ETest::WaitForAndInstrumentGlic() {
  MultiStep steps(Steps(
      UninstrumentWebContents(kGlicContentsElementId, false),
      UninstrumentWebContents(kGlicHostElementId, false),
      InAnyContext(
          ObserveState(kGlicInstanceCoordinatorState,
                       std::ref(instance_coordinator()), active_tab()),
          WaitForState(kGlicInstanceCoordinatorState, GlicPanelState::kOpen),
          Steps(InstrumentNonTabWebView(kGlicHostElementId, kGlicViewElementId),
                InstrumentInnerWebContents(kGlicContentsElementId,
                                           kGlicHostElementId, 0),
                WaitForWebContentsReady(kGlicContentsElementId)),
          StopObservingState(kGlicInstanceCoordinatorState))));

  AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
  return steps;
}

void GlicE2ETest::MaybeStartWebPageReplayForRecordingPath(
    const std::string recording_filename) {
  if (test_mode_ == kRealBackend && !use_wpr_for_real_backend_) {
    return;
  }
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
  base::FilePath recording_dir_path =
      base::MakeAbsoluteFilePath(root_path.Append(kRecordingDirectoryPath));
  base::FilePath recording_path = recording_dir_path.Append(
      base::FilePath::FromUTF8Unsafe(recording_filename));
  if (test_mode_ == kReplay ||
      (test_mode_ == kRealBackend && use_wpr_for_real_backend_)) {
    CHECK(base::PathExists(recording_path))
        << recording_filename << " does not exist.";
  }

  ASSERT_TRUE(web_page_replay_server_wrapper()->Start(recording_path));
}

GlicKeyedService* GlicE2ETest::glic_service() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(
      InProcessBrowserTest::browser()->GetProfile());
}
GlicInstanceCoordinator& GlicE2ETest::instance_coordinator() {
  return glic_service()->instance_coordinator();
}

WebPageReplayServerWrapper* GlicE2ETest::web_page_replay_server_wrapper() {
  return web_page_replay_server_wrapper_.get();
}

tabs::TabInterface* GlicE2ETest::active_tab() {
  return tabs::TabInterface::GetFromContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}

void GlicE2ETest::ThrottleCurrentTabNetwork() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);
  ThrottleWebContentsNetwork(web_contents);
}

void GlicE2ETest::ThrottleWebContentsNetwork(
    content::WebContents* web_contents) {
  CHECK(web_contents);

  auto& devtools_client_ptr = devtools_clients_[web_contents];
  if (!devtools_client_ptr) {
    devtools_client_ptr =
        std::make_unique<content::TestDevToolsProtocolClient>();
    devtools_client_ptr->AttachToWebContents(web_contents);
    devtools_client_ptr->SendCommand("Network.enable", base::DictValue());
  }

  // Corresponds to the "Slow 3G" preset in
  // third_party/devtools-frontend/src/front_end/core/sdk/NetworkManager.ts
  base::DictValue params;
  params.Set("offline", false);
  // Latency in ms.
  params.Set("latency", 2000.0);
  // Throughput in Bps.
  params.Set("downloadThroughput", 50000);
  params.Set("uploadThroughput", 50000);

  devtools_client_ptr->SendCommand("Network.emulateNetworkConditions",
                                   std::move(params));
}

void GlicE2ETest::ThrottleGlicNetwork() {
  auto& coordinator =
      static_cast<GlicInstanceCoordinatorImpl&>(instance_coordinator());
  for (GlicInstanceImpl* instance : coordinator.GetInstances()) {
    content::WebContents* guest_contents =
        instance->host().web_client_contents();
    if (guest_contents) {
      ThrottleWebContentsNetwork(guest_contents);
    }
  }
}

GlicActorTaskState::GlicActorTaskState(Profile* profile) {
  actor::ActorKeyedService* actor_keyed_service =
      actor::ActorKeyedService::Get(profile);
  CHECK(actor_keyed_service);
  actor_task_listener_ =
      actor_keyed_service->AddTaskStateChangedCallback(base::BindRepeating(
          &GlicActorTaskState::StateChanged, base::Unretained(this)));
}
GlicActorTaskState::~GlicActorTaskState() = default;

void GlicActorTaskState::StateChanged(actor::ActorTask& task) {
  if (task_id_.is_null()) {
    task_id_ = task.id();
  }
  if (task.id() != task_id_) {
    return;
  }
  OnStateObserverStateChanged(task.GetState());
}

DEFINE_STATE_IDENTIFIER_VALUE(GlicActorTaskState, kGlicActorTaskState);

const ui::ElementIdentifier kGlicHandoffButtonElementId =
    actor::ui::HandoffButtonController::kHandoffButtonElementId;

// Static assertions to ensure that commonly used ActorTask states in internal
// tests are validated on public bots to prevent silent build breakages.
static_assert(static_cast<int>(GlicActorTaskState::State::kFinished) >= 0);
static_assert(static_cast<int>(GlicActorTaskState::State::kCancelled) >= 0);
static_assert(static_cast<int>(GlicActorTaskState::State::kPausedByUser) >= 0);
static_assert(static_cast<int>(GlicActorTaskState::State::kReflecting) >= 0);

// Validate features and switches used by internal tests:
const base::Feature& GetGlicActionAllowlistFeature() {
  return actor::kGlicActionAllowlist;
}

const char* GetDisableActorSafetyChecksSwitch() {
  return actor::switches::kDisableActorSafetyChecks;
}

const base::Feature& GetGlicLiveModeFeature() {
  return features::kGlicLiveMode;
}

const base::Feature& GetGlicMultiInstanceFeature() {
  return features::kGlicMultiInstance;
}

// Validate Mojo types used by internal tests:
static_assert(static_cast<int>(actor::mojom::ActionResultCode::kOk) >= 0);

ui::ElementIdentifier GetGlicButtonElementId() {
  return kGlicButtonElementId;
}
ui::ElementIdentifier GetTabStripElementId() {
  return kTabStripElementId;
}
ui::ElementIdentifier GetOmniboxElementId() {
  return kOmniboxElementId;
}
ui::ElementIdentifier GetGlicViewElementId() {
  return kGlicViewElementId;
}

void GlicE2ETest::OnActiveInstanceChanged(GlicInstance* new_instance) {
  host_observation_.Reset();
  if (new_instance) {
    host_observation_.Observe(
        &static_cast<GlicInstanceImpl*>(new_instance)->host());
  }
}

void GlicE2ETest::WebUiStateChanged(glic::mojom::WebUiState state) {
  if (expects_error_) {
    return;
  }
  switch (state) {
    // Errors that should cause an early bail.
    case glic::mojom::WebUiState::kError:
    case glic::mojom::WebUiState::kUnresponsive:
    case glic::mojom::WebUiState::kGuestError:
    case glic::mojom::WebUiState::kDisabledByAdmin:
    case glic::mojom::WebUiState::kLocationMismatch:
    case glic::mojom::WebUiState::kIneligibleAccount:
    case glic::mojom::WebUiState::kOffline:
    case glic::mojom::WebUiState::kUnavailable: {
      ADD_FAILURE() << "Early bail: Glic WebUI entered error state: "
                    << static_cast<int>(state);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](base::WeakPtr<GlicE2ETest> self) {
                           if (self) {
                             self->instance_coordinator().Shutdown();
                           }
                         },
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    // Valid states for Glic where no early bail is needed.
    case glic::mojom::WebUiState::kUninitialized:
    case glic::mojom::WebUiState::kBeginLoad:
    case glic::mojom::WebUiState::kShowLoading:
    case glic::mojom::WebUiState::kHoldLoading:
    case glic::mojom::WebUiState::kFinishLoading:
    case glic::mojom::WebUiState::kReady:
    case glic::mojom::WebUiState::kWarmed:
    case glic::mojom::WebUiState::kSignIn:
      break;
  }
}

}  // namespace glic::test
