// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/entrypoint_controller.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/ttc/ttc_fre_dialog.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/views_switches.h"
#include "url/gurl.h"

namespace ttc {

namespace {

class EntrypointControllerInteractiveTest
    : public TtcInteractiveBrowserTestBase {
 public:
  EntrypointControllerInteractiveTest() = default;
  ~EntrypointControllerInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    TtcInteractiveBrowserTestBase::SetUpOnMainThread();
    profile()->GetPrefs()->SetInteger(prefs::kTtcFreAcceptedVersion,
                                      prefs::kCurrentFreVersion);
  }

 protected:
  StepBuilder SetToolbarButtonPinned(bool pinned) {
    return Do([this, pinned]() {
      CHECK_DEREF(PinnedToolbarActionsModel::Get(profile()))
          .UpdatePinnedState(kActionTtcToolbar, pinned);
    });
  }

  StepBuilder CheckToolbarButtonHighlighted(
      BrowserWindowInterface* target_browser,
      bool expected_highlighted) {
    return CheckResult(
        [target_browser]() {
          return CHECK_DEREF(BrowserWindow::FromBrowser(target_browser)
                                 ->GetPinnedToolbarActions())
              .IsActionHighlighted(kActionTtcToolbar);
        },
        expected_highlighted);
  }

  StepBuilder CheckToolbarButtonPoppedOut(
      BrowserWindowInterface* target_browser,
      bool expected_popped_out) {
    return CheckResult(
        [target_browser]() {
          return CHECK_DEREF(BrowserWindow::FromBrowser(target_browser)
                                 ->GetPinnedToolbarActions())
              .IsActionPoppedOut(kActionTtcToolbar);
        },
        expected_popped_out);
  }

  StepBuilder CheckAppMenuCommandEnabled(BrowserWindowInterface* target_browser,
                                         bool expected_enabled) {
    return CheckResult(
        [target_browser]() {
          return chrome::IsCommandEnabled(target_browser, IDC_SHOW_TTC_MENU);
        },
        expected_enabled);
  }

  StepBuilder CheckAppMenuActionEnabled(BrowserWindowInterface* target_browser,
                                        bool expected_enabled) {
    return CheckResult(
        [target_browser]() {
          const actions::ActionItem* const action =
              actions::ActionManager::Get().FindAction(
                  kActionShowTtcMenu,
                  CHECK_DEREF(BrowserActions::From(target_browser))
                      .root_action_item());
          return action && action->GetEnabled();
        },
        expected_enabled);
  }

  MultiStep CheckAppMenuEntrypointEnabled(
      BrowserWindowInterface* target_browser,
      bool expected_enabled) {
    return Steps(CheckAppMenuCommandEnabled(target_browser, expected_enabled),
                 CheckAppMenuActionEnabled(target_browser, expected_enabled));
  }

  StepBuilder InvokeAppMenuAction(BrowserWindowInterface* target_browser) {
    return Do([target_browser]() {
      CHECK_DEREF(actions::ActionManager::Get().FindAction(
                      kActionShowTtcMenu,
                      CHECK_DEREF(BrowserActions::From(target_browser))
                          .root_action_item()))
          .InvokeAction();
    });
  }

  MultiStep PinAndPressToolbarButton() {
    MultiStep steps = Steps(SetToolbarButtonPinned(true),
                            WaitForShow(kPinnedToolbarActionTtcElementId),
                            PressButton(kPinnedToolbarActionTtcElementId));
    AddDescriptionPrefix(steps, "PinAndPressToolbarButton()");
    return steps;
  }

  int AcceptedFreVersion() {
    return profile()->GetPrefs()->GetInteger(prefs::kTtcFreAcceptedVersion);
  }

  StepBuilder CheckFreAcceptedVersion(int expected_version) {
    return CheckResult([this]() { return AcceptedFreVersion(); },
                       expected_version, "CheckFreAcceptedVersion()");
  }
};

class EntrypointControllerAppMenuInteractiveTest
    : public EntrypointControllerInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  EntrypointControllerAppMenuInteractiveTest() {
    glow_up_feature_list_.InitWithFeatureState(features::kAppMenuGlowUp,
                                               GetParam());
  }
  ~EntrypointControllerAppMenuInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList glow_up_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         EntrypointControllerAppMenuInteractiveTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "GlowUpEnabled"
                                             : "GlowUpDisabled";
                         });

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       ActionItemRegistrationAndSessionToggle) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(browser(), false),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(browser(), true),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(false),
    CheckToolbarButtonHighlighted(browser(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       MultiWindowActionStateSync) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);
  const ui::ElementContext second_context =
      BrowserElements::From(second_browser)->GetContext();

  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(browser(), false),
    InContext(
      second_context,
      WaitForShow(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonHighlighted(second_browser, false)
    ),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(browser(), true),
    InContext(
      second_context,
      CheckToolbarButtonHighlighted(second_browser, true)
    ),

    InContext(
      second_context,
      PressButton(kPinnedToolbarActionTtcElementId)
    ),
    CheckHasSession(false),
    CheckToolbarButtonHighlighted(browser(), false),
    InContext(
      second_context,
      CheckToolbarButtonHighlighted(second_browser, false)
    )
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       UnpinnedButtonPopsOutDuringSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(false),
    WaitForHide(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), false),

    Do([this]() { ttc_service().StartSession(); }),
    CheckHasSession(true),

    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), true),
    CheckToolbarButtonHighlighted(browser(), true),

    Do([this]() { ttc_service().EndSession(); }),
    CheckHasSession(false),
    WaitForHide(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       NewBrowserWindowReflectsActiveSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(browser(), true)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  // clang-format off
  RunTestSequenceInContext(
    BrowserElements::From(second_browser)->GetContext(),

    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(second_browser, true),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuEntrypointStateFollowsSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),
    CheckAppMenuEntrypointEnabled(browser(), true),

    Do([this]() { ttc_service().StartSession(); }),
    CheckHasSession(true),
    CheckAppMenuEntrypointEnabled(browser(), false),

    Do([this]() { ttc_service().EndSession(); }),
    CheckHasSession(false),
    CheckAppMenuEntrypointEnabled(browser(), true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuCommandStartsSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    Do([this]() { chrome::ExecuteCommand(browser(), IDC_SHOW_TTC_MENU); }),
    CheckHasSession(true),
    CheckAppMenuCommandEnabled(browser(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuActionStartsSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    InvokeAppMenuAction(browser()),
    CheckHasSession(true),
    CheckAppMenuActionEnabled(browser(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuEntrypointStateSyncsAcrossWindows) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  // clang-format off
  RunTestSequence(
    CheckHasSession(false),
    CheckAppMenuEntrypointEnabled(browser(), true),
    CheckAppMenuEntrypointEnabled(second_browser, true),

    Do([this]() { ttc_service().StartSession(); }),
    CheckHasSession(true),
    CheckAppMenuEntrypointEnabled(browser(), false),
    CheckAppMenuEntrypointEnabled(second_browser, false),

    Do([this]() { ttc_service().EndSession(); }),
    CheckHasSession(false),
    CheckAppMenuEntrypointEnabled(browser(), true),
    CheckAppMenuEntrypointEnabled(second_browser, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       NewBrowserWindowAppMenuEntrypointReflectsActiveSession) {
  // clang-format off
  RunTestSequence(
    Do([this]() { ttc_service().StartSession(); }),
    CheckHasSession(true)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  // clang-format off
  RunTestSequence(
    CheckAppMenuEntrypointEnabled(second_browser, false)
  );
  // clang-format on
}

class EntrypointControllerFreInteractiveTest
    : public EntrypointControllerInteractiveTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EntrypointControllerInteractiveTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void SetUpOnMainThread() override {
    EntrypointControllerInteractiveTest::SetUpOnMainThread();
    profile()->GetPrefs()->ClearPref(prefs::kTtcFreAcceptedVersion);
  }
};

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       ShowsFreBeforeStartingSession) {
  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    CheckHasSession(false),
    CheckFreAcceptedVersion(0),
    CheckToolbarButtonHighlighted(browser(), false),
    CheckAppMenuEntrypointEnabled(browser(), true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       AcceptingFreStartsSession) {
  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(Steps(
      PressButton(TtcFreDialog::kAllowButtonElementId),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(true),
    CheckFreAcceptedVersion(prefs::kCurrentFreVersion),
    CheckToolbarButtonHighlighted(browser(), true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       DecliningFreDoesNotStartSession) {
  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(Steps(
      PressButton(TtcFreDialog::kCancelButtonElementId),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(false),
    CheckFreAcceptedVersion(0),
    CheckToolbarButtonHighlighted(browser(), false),

    PressButton(kPinnedToolbarActionTtcElementId),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       CompletedFreDoesNotShowAgain) {
  profile()->GetPrefs()->SetInteger(prefs::kTtcFreAcceptedVersion,
                                    prefs::kCurrentFreVersion);

  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    CheckHasSession(true),
    InAnyContext(EnsureNotPresent(TtcFreDialog::kDialogElementId))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       NewerFreVersionDoesNotShowAgain) {
  profile()->GetPrefs()->SetInteger(prefs::kTtcFreAcceptedVersion,
                                    prefs::kCurrentFreVersion + 1);

  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    CheckHasSession(true),
    InAnyContext(EnsureNotPresent(TtcFreDialog::kDialogElementId)),
    CheckFreAcceptedVersion(prefs::kCurrentFreVersion + 1)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       OutdatedFreVersionShowsAgain) {
  profile()->GetPrefs()->SetInteger(prefs::kTtcFreAcceptedVersion,
                                    prefs::kCurrentFreVersion - 1);

  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(Steps(
      PressButton(TtcFreDialog::kAllowButtonElementId),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(true),
    CheckFreAcceptedVersion(prefs::kCurrentFreVersion)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       ReinvokingEntrypointsKeepsSingleFre) {
  constexpr char kInitialFreDialog[] = "InitialFreDialog";

  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(NameElementRelative(
        TtcFreDialog::kDialogElementId, kInitialFreDialog,
        [](ui::TrackedElement* el) { return el; })),

    Do([this]() {
      EntrypointController& controller =
          CHECK_DEREF(EntrypointController::From(browser()));
      controller.EntrypointHandler(EntrypointType::kToolbarButton);
      controller.EntrypointHandler(EntrypointType::kAppMenu);
    }),
    EnsurePresent(kInitialFreDialog),
    InAnyContext(WaitForElementCount(TtcFreDialog::kDialogElementId, 1)),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       AppMenuShowsFreBeforeStartingSession) {
  // clang-format off
  RunTestSequence(
    Do([this]() { chrome::ExecuteCommand(browser(), IDC_SHOW_TTC_MENU); }),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    CheckHasSession(false),
    InSameContext(Steps(
      PressButton(TtcFreDialog::kAllowButtonElementId),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(true),
    CheckFreAcceptedVersion(prefs::kCurrentFreVersion)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       LearnMoreOpensHelpInNewWindow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHelpTabId);
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n", "",
              params->client.get());
          return true;
        }
        return false;
      }));

  // clang-format off
  RunTestSequence(
    InstrumentNextTab(kHelpTabId, AnyBrowser()),
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(DoDefaultAction(TtcFreDialog::kLearnMoreLinkElementId)),

    WaitForWebContentsReady(kHelpTabId, GURL("https://www.google.com/")),
    CheckResult([this]() { return browser()->GetTabStripModel()->count(); },
                1, "TabCountUnchanged"),
    InAnyContext(EnsurePresent(TtcFreDialog::kDialogElementId)),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       SessionStartingElsewhereClosesFre) {
  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(Steps(
      Do([this]() { ttc_service().StartSession(); }),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(true),
    CheckFreAcceptedVersion(0)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       AcceptingFreInOneWindowClosesItInTheOther) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);
  constexpr char kSecondWindowFreDialog[] = "SecondWindowFreDialog";

  // clang-format off
  RunTestSequence(
    Do([second_browser]() {
      chrome::ExecuteCommand(second_browser, IDC_SHOW_TTC_MENU);
    }),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId)),
    InSameContext(NameElementRelative(
        TtcFreDialog::kDialogElementId, kSecondWindowFreDialog,
        [](ui::TrackedElement* el) { return el; })),
    Do([this]() { chrome::ExecuteCommand(browser(), IDC_SHOW_TTC_MENU); }),
    InAnyContext(WaitForElementCount(TtcFreDialog::kDialogElementId, 2)),
    EnsurePresent(kSecondWindowFreDialog),
    InSameContext(Steps(
      PressButton(TtcFreDialog::kAllowButtonElementId),
      WaitForHide(TtcFreDialog::kDialogElementId))),

    CheckHasSession(true),
    InAnyContext(EnsureNotPresent(TtcFreDialog::kDialogElementId)),
    CheckFreAcceptedVersion(prefs::kCurrentFreVersion)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerFreInteractiveTest,
                       ClosingWindowWithFreDoesNotStartSession) {
  // clang-format off
  RunTestSequence(
    PinAndPressToolbarButton(),
    InAnyContext(WaitForShow(TtcFreDialog::kDialogElementId))
  );
  // clang-format on

  BrowserWindowInterface* const popup_browser =
      CreateBrowserForPopup(profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(popup_browser);

  EXPECT_FALSE(ttc_service().is_session_active());
  EXPECT_EQ(AcceptedFreVersion(), 0);
}

}  // namespace

}  // namespace ttc
