// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/e2e_test/glic_e2e_test.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if ENABLE_GLIC_INTERNAL_TESTS
#include "chrome/browser/glic/e2e_test/internal/constants.h"
#else
#include "chrome/browser/glic/e2e_test/internal_test_placeholder_constants.h"  // nogncheck
#endif

namespace glic::test {

namespace {

using glic::test::internal::kGlicFreShowingDialogState;
using glic::test::internal::kGlicWindowControllerState;

constexpr base::FilePath::StringViewType kRecordingDirectoryPath =
    FILE_PATH_LITERAL("chrome/browser/glic/e2e_test/internal/wpr_recordings");

const char kGlicE2ETestModeSwitch[] = "glic-e2e-test-mode";
const char kHostResolverRulesValue[] =
    "MAP *:80 127.0.0.1:8080,MAP *:443 127.0.0.1:8081,EXCLUDE localhost";

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

GlicE2ETest::GlicE2ETest() = default;
GlicE2ETest::~GlicE2ETest() = default;

void GlicE2ETest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                            features::kGlicKeyboardShortcutNewBadge,
                            features::kGlicRollout,
                            contextual_cueing::kContextualCueing},
      /*disabled_features=*/{});
  const base::CommandLine* command_line_of_test =
      base::CommandLine::ForCurrentProcess();

  std::string test_mode_value =
      command_line_of_test->GetSwitchValueASCII(kGlicE2ETestModeSwitch);

  if (test_mode_value.empty() || test_mode_value == "real_backend") {
    test_mode_ = kRealBackend;
  } else if (test_mode_value == "record") {
    test_mode_ = kRecord;
  } else if (test_mode_value == "replay") {
    test_mode_ = kReplay;
  } else {
    FAIL() << "Incorrect test mode input: %s" << test_mode_value;
  }

  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    web_page_replay_server_wrapper_ =
        std::make_unique<captured_sites_test_utils::WebPageReplayServerWrapper>(
            test_mode_ == kReplay, 8080, 8081, kWprArguments);
  }

  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  LiveTest::SetUp();
}

void GlicE2ETest::SetUpCommandLine(base::CommandLine* command_line) {
  LiveTest::SetUpCommandLine(command_line);

  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    // The following arguments make browser work with WPR proxy.
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    kHostResolverRulesValue);
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList,
        kIgnoreCertificateErrorsSPKIListValue);
  }
}

void GlicE2ETest::PreRunTestOnMainThread() {
  LiveTest::PreRunTestOnMainThread();

  GURL glic_fre_url = glic::GetFreURL(browser()->profile());
  GURL glic_guest_url = glic::GetGuestURL();
  CHECK(glic_fre_url.is_valid() && glic_guest_url.is_valid())
      << "Incorrect GLiC guest or FRE URL in cmd line arguments.";

  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    // When WPR is used, for consistency, require consistent host and path.
    CHECK(base::Contains(glic_fre_url.spec(), kAllowedHostAndPathForWpr) &&
          base::Contains(glic_guest_url.spec(), kAllowedHostAndPathForWpr))
        << "Please use allowed URL for WPR.";
  }
}

void GlicE2ETest::LoginTestAccountOrForceFakeSignin() {
  if (test_mode_ == kRealBackend || test_mode_ == kRecord) {
    std::optional<signin::TestAccountSigninCredentials> test_account =
        GetTestAccounts()->GetAccount(kTestAccountLabel);
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
    SigninWithPrimaryAccount(browser()->profile());
    SetModelExecutionCapability(browser()->profile(), true);
  }
}

void GlicE2ETest::SetFRECompletion() {
  ::glic::SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
}

void GlicE2ETest::SetUpInProcessBrowserTestFixture() {
  // Allowlists hosts.
  host_resolver()->AllowDirectLookup("*.google.com");

  LiveTest::SetUpInProcessBrowserTestFixture();
}

void GlicE2ETest::TearDownOnMainThread() {
  if (test_mode_ == kRecord || test_mode_ == kReplay) {
    // Ensure enough time for WPR to write archive at recording mode
    // by putting this in main thread.
    EXPECT_TRUE(web_page_replay_server_wrapper_->Stop())
        << "Cannot stop the local Web Page Replay server.";
  }
  LiveTest::TearDownOnMainThread();
}

ui::test::InteractiveTestApi::MultiStep GlicE2ETest::WaitForAndInstrumentFre() {
  MultiStep steps(Steps(
      UninstrumentWebContents(kGlicFreContentsElementId, false),
      UninstrumentWebContents(kGlicFreHostElementId, false),
      InAnyContext(ObserveState(kGlicFreShowingDialogState,
                                window_controller().fre_controller()),
                   WaitForState(kGlicFreShowingDialogState, true),
                   Steps(InstrumentNonTabWebView(
                             kGlicFreHostElementId,
                             GlicFreDialogView::kWebViewElementIdForTesting),
                         InstrumentInnerWebContents(kGlicFreContentsElementId,
                                                    kGlicFreHostElementId, 0),
                         WaitForWebContentsReady(kGlicFreContentsElementId)),
                   StopObservingState(kGlicFreShowingDialogState))));

  AddDescriptionPrefix(steps, "WaitForAndInstrumentFre");
  return steps;
}

ui::test::InteractiveTestApi::MultiStep
GlicE2ETest::WaitForAndInstrumentGlic() {
  MultiStep steps(Steps(
      UninstrumentWebContents(kGlicContentsElementId, false),
      UninstrumentWebContents(kGlicHostElementId, false),
      InAnyContext(
          ObserveState(kGlicWindowControllerState,
                       std::ref(window_controller())),
          WaitForState(kGlicWindowControllerState,
                       GlicWindowController::State::kOpen),
          Steps(InstrumentNonTabWebView(kGlicHostElementId, kGlicViewElementId),
                InstrumentInnerWebContents(kGlicContentsElementId,
                                           kGlicHostElementId, 0),
                WaitForWebContentsReady(kGlicContentsElementId)),
          StopObservingState(kGlicWindowControllerState))));

  AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
  return steps;
}

void GlicE2ETest::MaybeStartWebPageReplayForRecordingPath(
    const std::string recording_filename) {
  if (test_mode_ == kRealBackend) {
    return;
  }
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
  base::FilePath recording_dir_path =
      base::MakeAbsoluteFilePath(root_path.Append(kRecordingDirectoryPath));
  base::FilePath recording_path = recording_dir_path.Append(
      base::FilePath::FromUTF8Unsafe(recording_filename));
  if (test_mode_ == kReplay) {
    CHECK(base::PathExists(recording_path))
        << recording_filename << " does not exist.";
  }

  ASSERT_TRUE(web_page_replay_server_wrapper()->Start(recording_path));
}

GlicKeyedService* GlicE2ETest::glic_service() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(
      InProcessBrowserTest::browser()->GetProfile());
}
GlicWindowController& GlicE2ETest::window_controller() {
  return glic_service()->window_controller();
}
WebPageReplayServerWrapper* GlicE2ETest::web_page_replay_server_wrapper() {
  return web_page_replay_server_wrapper_.get();
}

}  // namespace glic::test
