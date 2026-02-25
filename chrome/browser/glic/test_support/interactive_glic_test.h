// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_

#include <algorithm>
#include <map>
#include <sstream>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
class GlicWindowControllerImpl;
}

namespace glic::test {

extern const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton;
extern const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel;

enum class TargetWebContents {
  kGlicWebUi,
  kGlicClient,
};

std::ostream& operator<<(std::ostream& os, const TargetWebContents& value);

// Mixin class that adds a mock glic to the current browser.
// If all you need is the combination of this + interactive browser test, use
// `InteractiveGlicTest` (defined below) instead.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest> &&
           std::derived_from<T, InteractiveBrowserTestApi>)
class InteractiveGlicTestMixin : public T {
 public:
  // Determines whether this is an attached or detached Glic window.
  // WARNING: This is no longer very meaningful, and should be replaced. These
  // do not provide the ability to open glic as a floating window when in
  // multi-instance mode. See the comments just below.
  enum GlicWindowMode {
    // Opens glic by pressing the Glic button on the browser.
    // In multi-instance, this means it will open glic as a side panel.
    // Otherwise, glic is opened as a floating window.
    kAttached,
    // Opens glic by calling ShowDetachedForTesting() on the window controller.
    // There may not be a good reason for using this.
    kDetached,
  };

  // What portions of the glic window should be instrumented on open.
  enum GlicInstrumentMode {
    // Instruments the host as `kGlicHostElementId` and contents as
    // `kGlicContentsElementId`.
    // WARNING, when SetUseElementIdentifiers(false) is used, these identifiers
    // are not instrumented.
    kHostAndContents,
    // Instruments only the host as `kGlicHostElementId`.
    // WARNING, when SetUseElementIdentifiers(false) is used, these identifiers
    // are not instrumented.
    kHostOnly,
    // Does not instrument either.
    kNone
  };
  using TargetWebContents = glic::test::TargetWebContents;

  // Constructor that takes `FieldTrialParams` and a
  // `GlicTestEnvironmentConfig`, then forwards the rest of the args.
  template <typename... Args>
  explicit InteractiveGlicTestMixin(
      const base::FieldTrialParams& glic_params,
      const GlicTestEnvironmentConfig& glic_config,
      Args&&... args)
      : T(std::forward<Args>(args)...), glic_test_environment_(glic_config) {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlic, glic_params},
         {features::kGlicRollout, {}},
         {features::kGlicKeyboardShortcutNewBadge, {}},
#if BUILDFLAG(IS_CHROMEOS)
         { chromeos::features::kFeatureManagementGlic,
           {} }
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  // Default constructor (no forwarded args or field trial parameters).
  InteractiveGlicTestMixin()
      : InteractiveGlicTestMixin(base::FieldTrialParams(),
                                 GlicTestEnvironmentConfig()) {}

  explicit InteractiveGlicTestMixin(const base::FieldTrialParams& glic_params)
      : InteractiveGlicTestMixin(glic_params, GlicTestEnvironmentConfig()) {}

  // Constructor with no field trial params; all arguments are forwarded to the
  // base class.
  template <typename Arg, typename... Args>
    requires(!std::same_as<base::FieldTrialParams, std::remove_cvref_t<Arg>>)
  explicit InteractiveGlicTestMixin(Arg&& arg, Args&&... args)
      : InteractiveGlicTestMixin(base::FieldTrialParams(),
                                 std::forward<Arg>(arg),
                                 std::forward<Args>(args)...) {}

  ~InteractiveGlicTestMixin() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    T::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpOnMainThread() override {
    LOG(INFO) << "InteractiveGlicTest: setting up base fixture";
    T::SetUpOnMainThread();
    instance_tracker_.SetProfile(T::GetProfile());
    LOG(INFO) << "InteractiveGlicTest: setting up";
    CHECK(glic_test_environment_.SetupEmbeddedTestServers(
        Test::embedded_test_server(), &Test::embedded_https_test_server()));

    LOG(INFO) << "InteractiveGlicTest: done setting up";

    SidePanelCoordinator::From(browser())->DisableAnimationsForTesting();
  }

  void TearDownOnMainThread() override {
    instance_tracker_.SetProfile(nullptr);
    T::TearDownOnMainThread();
  }

  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return WaitForAndInstrumentGlicMultiInstance(instrument_mode);
    }
    return WaitForAndInstrumentGlic(instrument_mode, window_controller());
  }

  auto WaitForAndInstrumentGlicMultiInstance(
      GlicInstrumentMode instrument_mode) {
    if (!use_element_identifiers_) {
      return WaitForGlic(instrument_mode);
    }
    Api::MultiStep steps;
    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::InAnyContext(
                Api::Steps(Api::InstrumentNonTabWebView(kGlicHostElementId,
                                                        kGlicViewElementId),
                           Api::InstrumentInnerWebContents(
                               kGlicContentsElementId, kGlicHostElementId, 0),
                           Api::Log("Waiting for Glic web contents ready"),
                           Api::WaitForWebContentsReady(kGlicContentsElementId),
                           Api::Log("Glic web contents is ready"))),
            WaitUntil(
                [this]() -> std::string {
                  GlicInstance* instance = GetGlicInstanceImpl();
                  if (!instance) {
                    return "No glic instance for " +
                           instance_tracker_.DescribeGlicTracking();
                  }
                  if (!instance->IsShowing()) {
                    return "Glic not showing";
                  }
                  if (!instance->host().IsReady()) {
                    return "Glic host not ready";
                  }
                  return "showing and ready";
                },
                "showing and ready", "WaitForReadyAndShowing"));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
      default:
        NOTREACHED();
    }
    return steps;
  }

  auto WaitForGlic(GlicInstrumentMode instrument_mode) {
    Api::MultiStep steps;
    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(WaitForGlicOpen(),
                           WaitForWebUIState(mojom::WebUiState::kReady));
        break;
      case GlicInstrumentMode::kHostOnly:
        steps = Api::Steps(WaitForGlicOpen());
        break;
      default:
        break;
    }
    return steps;
  }

  // Ensures that the WebContents for some combination of glic host and contents
  // are instrumented, per `instrument_mode`. Takes a window controller, to
  // permit instrumenting for a different profile.
  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode,
                                GlicWindowController& window_controller) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;

    if (!use_element_identifiers_) {
      return WaitForGlic(instrument_mode);
    }

    // NOTE: When the kGlicMultiInstance feature is enabled, the active tab is
    // passed to the kGlicWindowControllerState observer so it observes the
    // relevant GlicInstance.
    tabs::TabInterface* active_tab = nullptr;
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      active_tab = browser()->tab_strip_model()->GetActiveTab();
    }

    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller),
                              std::move(active_tab)),
            Api::InAnyContext(Api::Steps(
                Api::InstrumentNonTabWebView(kGlicHostElementId,
                                             kGlicViewElementId),
                Api::InstrumentInnerWebContents(kGlicContentsElementId,
                                                kGlicHostElementId, 0),
                Api::WaitForWebContentsReady(kGlicContentsElementId))),
            Api::WaitForState(internal::kGlicWindowControllerState,
                              GlicWindowController::State::kOpen),
            Api::StopObservingState(internal::kGlicWindowControllerState)
            /*, WaitForElementVisible(kPathToGuestPanel)*/);
        break;
      case GlicInstrumentMode::kHostOnly:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller),
                              std::move(active_tab)),
            Api::InAnyContext(Api::InstrumentNonTabWebView(kGlicHostElementId,
                                                           kGlicViewElementId)),
            Api::WaitForState(
                internal::kGlicWindowControllerState,
                testing::Matcher<GlicWindowController::State>(testing::AnyOf(
                    GlicWindowController::State::kWaitingForGlicToLoad,
                    GlicWindowController::State::kOpen))),
            Api::StopObservingState(internal::kGlicWindowControllerState));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
    }
    Api::AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
    return steps;
  }

  // Activate one of the glic entrypoints.
  // In single-instance, this will open floaty, in multi-instance it will open
  // side panel.
  auto OpenGlic(GlicInstrumentMode instrument_mode =
                    GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps =
        Api::Steps(Api::Log("Opening glic window"), CheckGlicIsClosed(),
                   // Technically, this toggles the window, but we've
                   // already ensured that it's closed.
                   ToggleGlicWindow(GlicWindowMode::kDetached),
                   WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // Activate one of the glic entrypoints.
  // If `instrument_glic_contents` is true both the host and contents will be
  // instrumented (see `WaitForAndInstrumentGlic()`) else only the host will be
  // instrumented (`WaitForAndInstrumentGlicHostOnly()`).
  auto DeprecatedOpenGlicWindow(GlicWindowMode window_mode,
                                GlicInstrumentMode instrument_mode =
                                    GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps =
        Api::Steps(Api::Log("Opening glic window"), CheckGlicIsClosed(),
                   // Technically, this toggles the window, but we've
                   // already ensured that it's closed.
                   ToggleGlicWindow(window_mode),
                   WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // A test step which runs `fn()` until it returns `desired_result`.
  template <typename F>
  auto WaitUntil(F fn,
                 std::string desired_result,
                 std::string description = "") {
    auto fn_callback = ui::test::internal::MaybeBindRepeating(fn);
    auto wrapped = [](const auto& fn, std::string desired_result,
                      std::string description,
                      std::optional<std::string>& last_result,
                      std::optional<base::TimeTicks>& last_log_time) {
      auto result = fn.Run();
      if (result == desired_result) {
        return true;
      }
      if (last_result) {
        bool should_print =
            *last_result != result || !last_log_time ||
            *last_log_time < base::TimeTicks::Now() - base::Seconds(1);
        if (should_print) {
          LOG(WARNING) << "WaitUntil(" << description
                       << ") still waiting. Received value " << result
                       << " != " << desired_result;
          last_log_time = base::TimeTicks::Now();
        }
      }

      last_result = result;
      return false;
    };
    base::RepeatingCallback<bool()> callback = base::BindRepeating(
        wrapped, base::OwnedRef(std::move(fn_callback)), desired_result,
        description, base::OwnedRef(std::optional<std::string>()),
        base::OwnedRef(std::optional<base::TimeTicks>()));
    return Api::PollUntil(callback, description);
  }

  auto WaitForGlicOpen() {
    return WaitUntil(
        [this]() -> std::string {
          auto* instance = GetGlicInstance();
          if (!instance) {
            return "No instance";
          }
          if (instance->IsShowing()) {
            return "showing";
          }
          return "not showing";
        },
        "showing", "WaitForGlicOpen");
  }

  auto WaitForGlicClose() {
    Api::MultiStep steps;
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance) ||
        !use_element_identifiers_) {
      Api::AddStep(steps, WaitUntil(
                              [this]() {
                                auto* instance = GetGlicInstance();
                                if (!instance || !instance->IsShowing()) {
                                  return "hidden";
                                }
                                return "showing";
                              },
                              "hidden", "WaitForGlicClose"));
    } else {
      Api::AddStep(steps,
                   Api::InAnyContext(Api::WaitForHide(kGlicViewElementId)));
    }
    Api::AddDescriptionPrefix(steps, "WaitForGlicClose");
    return steps;
  }

  auto OpenGlicFloatingWindow(GlicInstrumentMode instrument_mode =
                                  GlicInstrumentMode::kHostAndContents) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      auto steps = Api::Steps(
          Api::Do([this]() {
            GetInstanceCoordinator().Toggle(
                /*browser=*/nullptr, true, mojom::InvocationSource::kOsButton,
                /*prompt_suggestion=*/std::nullopt,
                /*auto_send=*/false,
                /*conversation_id=*/std::nullopt);
          }),
          WaitForAndInstrumentGlic(instrument_mode), WaitForGlicOpen());
      Api::AddDescriptionPrefix(steps, "OpenGlicFloatingWindow");
      return steps;
    } else {
      return OpenGlic(instrument_mode);
    }
  }

  // Toggles Glic through one of the entrypoints.
  // Does not wait for Glic to open or close, tests using this should check for
  // the correct window state after toggling.
  auto ToggleGlicWindow(GlicWindowMode window_mode) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::PressButton(kGlicButtonElementId)
          .SetContext(BrowserElements::From(browser())->GetContext());
    }
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(kGlicButtonElementId)
            .SetContext(BrowserElements::From(browser())->GetContext());
      case GlicWindowMode::kDetached:
        return Api::Do(
            [this] { window_controller().ShowDetachedForTesting(); });
    }
  }

  // Toggles Glic through a specific InvocationSource.
  auto ToggleGlicWindowFromSource(GlicWindowMode window_mode,
                                  ui::ElementIdentifier element_id,
                                  mojom::InvocationSource invocation_source) {
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(element_id);
      case GlicWindowMode::kDetached:
        return Api::Do([this, invocation_source] {
          window_controller().Toggle(browser(), false, invocation_source,
                                     /*prompt_suggestion=*/std::nullopt,
                                     /*auto_send=*/false,
                                     /*conversation_id=*/std::nullopt);
        });
    }
  }

  // Close the glic panel, regardless of the current state. Unlike
  // `CloseGlicWindow()`, this will close the window even if the glic client is
  // not connected, and will do nothing if the window is already closed.
  auto CloseGlic() {
    return Api::Do([this]() {
      if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
        auto* instance = GetGlicInstanceImpl();
        if (!instance) {
          return;
        }
        instance->CloseAllEmbedders();
      } else {
        window_controller().Close(CloseOptions());
      }
    });
  }

  auto RegisterConversation(std::string conversation_id) {
    return Api::Do([this, conversation_id]() {
      if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
        auto* instance = GetGlicInstanceImpl();
        if (!instance) {
          return;
        }
        auto conversation_info = glic::mojom::ConversationInfo::New();
        conversation_info->conversation_id = conversation_id;
        instance->RegisterConversation(std::move(conversation_info),
                                       base::DoNothing());
      }
    });
  }

  auto ClickWebuiCloseButton() {
    return ClickWebElement(TargetWebContents::kGlicWebUi, ".close-button");
  }

  auto Detach() {
    return Api::Do([this]() {
      auto* host = GetHost();
      CHECK(host);
      host->DetachPanel(host->GetPrimaryPageHandlerForTesting());
    });
  }

  base::expected<content::RenderFrameHost*, std::string> GetWebFrame(
      TargetWebContents target) {
    auto* host = GetHost();
    if (!host) {
      return base::unexpected("GetWebFrame: no host");
    }
    switch (target) {
      case TargetWebContents::kGlicWebUi: {
        auto* host_contents = host->webui_contents();
        if (!host_contents) {
          return base::unexpected("GetWebFrame: no host web contents");
        }
        auto* frame = host_contents->GetPrimaryMainFrame();
        if (!frame) {
          return base::unexpected("GetWebFrame: no primary main frame");
        }
        return base::ok(frame);
      }
      case TargetWebContents::kGlicClient: {
        if (host->GetPageHandlersForTesting().empty()) {
          return base::unexpected("GetWebFrame: no page handlers");
        }
        auto* frame = FindGlicGuestMainFrame();
        if (!frame) {
          return base::unexpected("GetWebFrame: no client frame");
        }
        return frame;
      }
    }
  }

  auto WaitForWebElementShown(TargetWebContents contents, std::string query) {
    std::string script = R"js(
    (()=>{
      const el = document.querySelector(`$1`);
      if (!el) { return `$1 element not found`; }
      if (!el.checkVisibility()) { return `$1 element not visible`; }
      return "shown";
    })()
    )js";
    script = base::ReplaceStringPlaceholders(
        script, base::span<const std::string>({query}), nullptr);
    return WaitUntil(
        base::BindRepeating(base::BindLambdaForTesting(
                                [this](TargetWebContents contents,
                                       std::string script) -> std::string {
                                  auto frame = GetWebFrame(contents);
                                  CHECK(frame.has_value()) << frame.error();
                                  auto result =
                                      content::EvalJs(frame.value(), script);
                                  return result.ExtractString();
                                }),
                            contents, script),
        "shown");
  }

  auto ClickWebElement(TargetWebContents contents,
                       std::string query,
                       bool wait = true) {
    std::string script = R"js(
    (()=>{
      const wait = $2;
      if (wait) {
        document.querySelector(`$1`).click();
      } else {
        setTimeout(() => document.querySelector(`$1`).click(), 100);
      }
    })()
    )js";
    script = base::ReplaceStringPlaceholders(
        script, base::span<const std::string>({query, wait ? "true" : "false"}),
        nullptr);

    return Api::Steps(
        WaitForWebElementShown(contents, query),
        Api::Check(base::BindLambdaForTesting([this, contents, script]() {
          bool ok = content::ExecJs(GetWebFrame(contents).value(), script);
          return ok;
        })));
  }

  // Ensures a mock glic button is present and then clicks it. Works even if the
  // element is off-screen.
  auto ClickMockGlicElement(
      const WebContentsInteractionTestUtil::DeepQuery& where,
      const bool click_closes_window = false) {
    auto steps = Api::Steps();
    if (!use_element_identifiers_) {
      steps = Api::Steps(
          WaitForGlicOpen(), WaitForWebUIState(mojom::WebUiState::kReady),
          WaitUntil(
              [this]() -> std::string {
                auto* handler = GetHost()->GetPrimaryPageHandlerForTesting();
                if (!handler) {
                  return "no page handler";
                }
                if (!handler->GetGuestMainFrame()) {
                  return "no guest frame";
                }
                return "ok";
              },
              "ok"),
          Api::Do([this, where = where]() {
            auto* handler = GetHost()->GetPrimaryPageHandlerForTesting();
            CHECK(handler) << "No page handler";
            auto* frame = handler->GetGuestMainFrame();
            CHECK(frame) << "No guest frame";
            CHECK(content::ExecJs(frame, "()=>document.querySelector(\"" +
                                             where[0] + "\").click()"));
          }));
    } else {
      steps = Api::Steps(
          // Note: Elements on the test client don't need to be in the viewport
          // to be used. Ideally we would wait until the element is visible, but
          // not necessarily on screen. Because we don't have any elements that
          // get hidden on the test client, waiting for body visibility is good
          // enough.
          Api::WaitForElementVisible(kGlicContentsElementId, {"body"}),
          // TODO(dfried): Figure out why Api::CheckJsResultAt() here doesn't
          // work. Error:
          // Interactive test failed on step 28 (ClickMockGlicElement:
          // CheckJsResultAt( {"#contextAccessIndicator"}, " ... with reason
          // kSequenceDestroyed; step type kShown; id ElementIdentifier
          // kGlicContentsElementId.
          Api::ExecuteJsAt(
              kGlicContentsElementId, where, "(el)=>el.click()",
              click_closes_window
                  ? InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget
                  : InteractiveBrowserTestApi::ExecuteJsMode::
                        kWaitForCompletion));
    }

    Api::AddDescriptionPrefix(steps, "ClickMockGlicElement");
    return steps;
  }

  // Closes the glic window, which must be open.
  //
  // TODO: this only works if glic is actually loaded; handle the case where the
  // contents pane has either not loaded or failed to load.
  auto CloseGlicWindow() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::InAnyContext(Api::Steps(CheckGlicInstanceIsShowing(),
                                              CloseGlic(), WaitForGlicClose()));
    Api::AddDescriptionPrefix(steps, "CloseGlicWindow");
    return steps;
  }

  auto SimulateAcceleratorPress(const ui::Accelerator& accelerator) {
    return Api::Do([this, accelerator] {
      CHECK(GetGlicWidget());
      gfx::NativeWindow target_window = GetGlicWidget()->GetNativeWindow();
#if (USE_AURA)
      ui::test::EventGenerator event_generator(target_window->GetRootWindow(),
                                               target_window);
#else
      ui::test::EventGenerator event_generator(target_window);
#endif
      event_generator.set_target(ui::test::EventGenerator::Target::WINDOW);
      event_generator.PressAndReleaseKeyAndModifierKeys(
          accelerator.key_code(), accelerator.modifiers());
    });
  }

  auto CheckControllerHasWidget(bool expect_widget) {
    return Api::CheckResult([this]() { return GetGlicWidget() != nullptr; },
                            expect_widget, "CheckControllerHasWidget");
  }

  auto CheckControllerShowing(bool expect_showing) {
    return Api::CheckResult(
        [this]() {
          if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
            return GetGlicInstance() && GetGlicInstance()->IsShowing();
          } else {
            return GetWindowControllerImpl().IsShowing();
          }
        },
        expect_showing, "CheckControllerShowing");
  }

  auto CheckControllerWidgetMode(GlicWindowMode mode) {
    return Api::CheckResult(
        [this]() {
          if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
            if (!GetGlicInstance()) {
              return GlicWindowMode::kAttached;
            }
            return GetGlicInstance()->IsAttached() ? GlicWindowMode::kAttached
                                                   : GlicWindowMode::kDetached;
          } else {
            return GetWindowControllerImpl().IsAttached()
                       ? GlicWindowMode::kAttached
                       : GlicWindowMode::kDetached;
          }
        },
        mode, "CheckControllerWidgetMode");
  }

  auto CheckIfAttachedToBrowser(Browser* new_browser) {
    return Api::CheckResult(
        [this] { return window_controller().attached_browser(); }, new_browser,
        "attached to the other browser");
  }

  auto CheckTabCount(int expected_count) {
    return Api::CheckResult(
        [this] { return browser()->tab_strip_model()->count(); },
        expected_count, "CheckTabCount");
  }

  // Opens a new incognito browser to keep the test process alive, then closes
  // the main browser.
  void CloseMainBrowserWithIncognitoKeepAlive() {
    T::CreateIncognitoBrowser();
    T::CloseBrowserAsynchronously(T::browser());
  }

  auto CheckPopupCount(int expected_count) {
    return Api::CheckResult(
        [] {
          int popup_count = 0;
          GlobalBrowserCollection::GetInstance()->ForEach(
              [&popup_count](BrowserWindowInterface* browser) {
                if (browser->GetType() == BrowserWindowInterface::TYPE_POPUP) {
                  popup_count++;
                }
                return true;
              });
          return popup_count;
        },
        expected_count, "CheckPopupCount");
  }

  auto CheckOcclusionTracked(bool expect_is_tracked) {
    return Api::CheckResult(
        [this]() {
          return std::ranges::contains(
              PictureInPictureWindowManager::GetInstance()
                  ->GetOcclusionTracker()
                  ->GetPictureInPictureWidgetsForTesting(),
              GetGlicWidget());
        },
        expect_is_tracked, "CheckOcclusionTracked");
  }

  auto Wait(base::TimeDelta timeout) {
    auto observer = std::make_unique<internal::WaitingStateObserver>();
    auto observer_ptr = observer.get();
    return Api::Steps(
        Api::Do(base::BindRepeating(
            [](internal::WaitingStateObserver* observer,
               base::TimeDelta timeout) { observer->Start(timeout); },
            base::Unretained(observer_ptr), timeout)),
        Api::ObserveState(glic::test::internal::kDelayState,
                          std::move(observer)),
        Api::WaitForState(glic::test::internal::kDelayState, true));
  }

  auto WaitForCanResizeEnabled(bool enabled) {
    return Api::Steps(
        Api::ObserveState(internal::kGlicWindowControllerResizeState,
                          std::ref(window_controller())),
        Api::Log("WaitForCanResize: ", enabled ? "true" : "false"),
        Api::WaitForState(internal::kGlicWindowControllerResizeState, enabled),
        Api::StopObservingState(internal::kGlicWindowControllerResizeState));
  }

  content::RenderFrameHost* FindGlicGuestMainFrame() {
    Host* host = GetHost();
    if (!host) {
      return nullptr;
    }
    for (GlicPageHandler* handler : GetHost()->GetPageHandlersForTesting()) {
      if (handler->GetGuestMainFrame()) {
        return handler->GetGuestMainFrame();
      }
    }
    return nullptr;
  }

  content::WebContents* FindGlicWebUIContents() {
    Host* host = GetHost();
    return host ? host->webui_contents() : nullptr;
  }

  glic::GlicTestEnvironment& glic_test_environment() {
    return glic_test_environment_;
  }

  glic::GlicTestEnvironmentService& glic_test_service() {
    return *glic_test_environment_.GetService(browser()->GetProfile());
  }

  // Send a task state update to show the actor task icon in the tab strip.
  void StartTaskAndShowActorTaskIcon() {
    auto actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
    actor::TaskId task_id =
        actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
    actor::ui::StartTask start_task_event(task_id);
    actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);
  }

  void ReloadGlicWebui() {
    Host* host = GetHost();
    CHECK(host);
    host->Reload();
  }

  void DisableWarming() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GetInstanceCoordinator().SetWarmingEnabledForTesting(false);
    } else {
      // Not supported for single-instance, as warming is disabled by feature
      // flag.
    }
  }

  // Same as `Api::AddInstrumentedTab()`, but also opens a side panel from the
  // currently tracked instance if the Multi-Instance flag is enabled.
  InteractiveBrowserTestApi::MultiStep AddInstrumentedTabAndOpenSidePanel(
      ui::ElementIdentifier id,
      GURL url,
      std::optional<int> at_index = std::nullopt) {
    auto steps = Api::Steps(
        Api::InstrumentNextTab(id),
        Api::WithElement(
            ui::test::internal::kInteractiveTestPivotElementId,
            base::BindLambdaForTesting([this, url](ui::TrackedElement* el) {
              Browser* const browser_ptr = browser();
              CHECK(browser_ptr) << "No browser";
              CHECK(GetHost());
              GetHost()->instance_delegate().CreateTab(
                  url, /*open_in_background=*/false,
                  /*window_id=*/std::nullopt, base::DoNothing());
            })),
        Api::WaitForWebContentsReady(id));
    Api::AddDescriptionPrefix(
        steps,
        base::StringPrintf("AddInstrumentedTabAndOpenSidePanel( %s, %s, %d, )",
                           id.GetName().c_str(), url.spec().c_str(),
                           at_index.value_or(-1)));
    return steps;
  }

  auto WaitForWebUIState(mojom::WebUiState state) {
    std::stringstream ss;
    ss << state;
    return WaitUntil(
        [this]() -> std::string {
          auto* instance = GetGlicInstance();
          if (!instance) {
            return "no instance";
          }
          std::stringstream ss;
          ss << instance->host().GetPrimaryWebUiState();
          return ss.str();
        },
        ss.str(), "WaitForWebUIState");
  }

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  GlicWindowControllerImpl& GetWindowControllerImpl() {
    CHECK(!base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicWindowControllerImpl&>(
        glic_service()->window_controller());
  }

  GlicInstanceCoordinatorImpl& GetInstanceCoordinator() {
    CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicInstanceCoordinatorImpl&>(
        glic_service()->window_controller());
  }

  GlicInstanceImpl* GetGlicInstanceImpl() {
    CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicInstanceImpl*>(GetGlicInstance());
  }

  views::View* GetGlicView() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GlicInstanceImpl* instance = GetGlicInstanceImpl();
      if (!instance) {
        return nullptr;
      }
      return instance->GetActiveEmbedderGlicViewForTesting();
    }

    return GetWindowControllerImpl().GetGlicViewForTesting();
  }

  views::Widget* GetGlicWidget() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GlicInstanceImpl* instance = GetGlicInstanceImpl();
      if (!instance) {
        return nullptr;
      }
      views::View* view = instance->GetActiveEmbedderGlicViewForTesting();
      if (!view) {
        return nullptr;
      }
      return view->GetWidget();
    }
    return window_controller().GetGlicWidget();
  }

  Host* GetHost() {
    GlicInstance* instance = GetGlicInstance();
    if (!instance) {
      return nullptr;
    }
    return &instance->host();
  }

  auto CheckGlicInstanceIsShowing() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::CheckResult(
          [this]() {
            auto* instance = GetGlicInstance();
            return instance && instance->IsShowing();
          },
          "glic panel must be open");
    }
    return EnsureGlicWindowState("glic window must be open",
                                 GlicWindowController::State::kOpen);
  }
  auto CheckGlicIsClosed() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::CheckResult(
          [this]() {
            views::View* view = GetGlicView();
            return !view || !view->GetVisible();
          },
          "glic panel must be closed");
    }
    return EnsureGlicWindowState("glic window must be closed",
                                 GlicWindowController::State::kClosed);
  }

  template <typename... M>
  auto EnsureGlicWindowState(const std::string& desc, M&&... matchers) {
    return Api::CheckResult([this]() { return window_controller().state(); },
                            testing::Matcher<GlicWindowController::State>(
                                testing::AnyOf(std::forward<M>(matchers)...)),
                            desc);
  }

  void SetGlicPagePath(const std::string& glic_page_path) {
    glic_test_environment_.SetGlicPagePath(glic_page_path);
  }

  // Adds a query param to the URL that will be used to load the mock glic.
  // Must be called before `SetUpOnMainThread()`. Both `key` and `value` (if
  // specified) will be URL-encoded for safety.
  void AddMockGlicQueryParam(const std::string_view& key,
                             const std::string_view& value = "") {
    glic_test_environment_.AddMockGlicQueryParam(key, value);
  }

  GURL GetGuestURL() { return glic_test_environment_.GetGuestURL(); }

  void SetGlicFreUrlOverride(const GURL& url) {
    glic_test_environment_.SetGlicFreUrlOverride(url);
  }

  // `InteractiveGlicTestMixin` is configured to operate a single browser, but
  // it can change which browser it operates. This changes the browser to be
  // used in functions of `InteractiveGlicTestMixin`.
  void SetActiveBrowser(Browser* browser) {
    active_browser_ = browser->AsWeakPtr();
  }

  // Returns the active browser.
  Browser* browser() {
    if (active_browser_) {
      return active_browser_.get();
    } else {
      CHECK(!active_browser_.WasInvalidated())
          << "SetActiveBrowser() was called, but that browser no longer "
             "exists.";
      return InProcessBrowserTest::browser();
    }
  }

  // Glic tracking functions. By default, this fixture applies operations toward
  // the glic instance in tab 0. You can change this behavior by calling one of
  // these functions.

  // Have all glic instance operations linked to a glic instance with this ID.
  void TrackGlicInstanceWithId(InstanceId id) {
    instance_tracker_.TrackGlicInstanceWithId(id);
  }

  // Track the glic instance at a specific tab index.
  void TrackGlicInstanceWithTabIndex(int index) {
    instance_tracker_.TrackGlicInstanceWithTabIndex(index);
  }

  // Track the glic instance at this tab.
  void TrackGlicInstanceWithTabHandle(tabs::TabInterface::Handle handle) {
    instance_tracker_.TrackGlicInstanceWithTabHandle(handle);
  }

  void TrackFloatingGlicInstance() {
    instance_tracker_.TrackFloatingGlicInstance();
  }

  void TrackOnlyGlicInstance() { instance_tracker_.TrackOnlyGlicInstance(); }

  // Returns the currently tracked glic instance.
  GlicInstance* GetGlicInstance() {
    return instance_tracker_.GetGlicInstance();
  }

  // Whether to use `ui::ElementIdentifier`s, or an alternative implementation
  // which avoids them. With GlicMultiInstance, it can be tricky to track the
  // element identifiers for the glic window, because the window is created
  // and destroyed during attach/detach/close. Additionally, the active instance
  // can change fluidly, so avoiding element identifiers lets us exclusively use
  // GlicInstanceTracker to determine which instance the test cares about.
  // Turning this off is currently experimental.
  // Many, but not all, of the verbs in this fixture will work when this is
  // disabled.
  void SetUseElementIdentifiers(bool use_element_identifiers) {
    use_element_identifiers_ = use_element_identifiers;
  }

 private:
  bool use_element_identifiers_ = true;
  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  // These determine which glic instance is tracked by this class. This affects
  // many functions in this fixture. Only one will be present at a time.
  GlicInstanceTracker instance_tracker_;

  base::WeakPtr<Browser> active_browser_;
  glic::GlicTestEnvironment glic_test_environment_;
  // This is the default test file. Tests can override with a different path.
  base::test::ScopedFeatureList features_;
};

// For most tests, you can alias or inherit from this instead of deriving your
// own `InteractiveGlicTestMixin<...>`.
using InteractiveGlicTest = InteractiveGlicTestMixin<InteractiveBrowserTest>;

// For testing IPH associated with glic - i.e. help bubbles that anchor in the
// chrome browser rather than showing up in the glic content itself - inherit
// from this.
using InteractiveGlicFeaturePromoTest =
    InteractiveGlicTestMixin<InteractiveFeaturePromoTest>;

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
