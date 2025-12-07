// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_INTERACTIVE_UITEST_COMMON_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_INTERACTIVE_UITEST_COMMON_H_

#include <memory>
#include <optional>
#include <string_view>
#include <variant>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

// Tests the actor framework using the Glic API surface. This tests is meant to
// exercise the API and end-to-end plumbing within Chrome. These tests aim to
// faithfully mimic Glic's usage of these APIs to provide some basic coverage
// that changes in Chrome aren't breaking Glic (though this relies on manual
// intervention anytime Glic changes and so is not a replacement for full
// end-to-end tests).
class GlicActorUiTest : public test::InteractiveGlicTest {
 public:
  using InteractiveTestApi::MultiStep;
  using ActionProtoProvider = base::OnceCallback<std::string()>;
  using ExpectedErrorResult = std::variant<std::monostate,
                                           actor::mojom::ActionResultCode,
                                           mojom::PerformActionsErrorReason>;
  using ExpectedResumeResult =
      std::variant<std::monostate, actor::mojom::ActionResultCode, bool>;

  static constexpr int32_t kNonExistentContentNodeId =
      std::numeric_limits<int32_t>::max();
  static constexpr char kActivateSurfaceIncompatibilityNotice[] =
      "Programmatic window activation does not work on the Weston reference "
      "implementation of Wayland used on Linux testbots. It also doesn't work "
      "reliably on Linux in general. For this reason, some of these tests "
      "which "
      "use ActivateSurface() may be skipped on machine configurations which do "
      "not reliably support them.";

  GlicActorUiTest();
  ~GlicActorUiTest() override;

  void SetUpOnMainThread() override;

  const actor::ActorTask* GetActorTask();

  // Returns the WebContents of the Glic guest.
  content::WebContents* GetGlicContents();

  // Returns the WebContents of the Glic host (WebUI).
  content::WebContents* GetGlicHost(actor::TaskId& task_id);

  // Executes a BrowserAction and verifies it succeeds. Optionally takes an
  // error reason which, when provided, causes failure if the action is
  // successful or fails with an unexpected reason.
  //
  // The action is passed as a proto "provider" which is a callback that returns
  // a string which is the base-64 representation of the BrowserAction proto to
  // invoke. This is a callback rather than a BrowserAction since, in some
  // cases, the parameters in the proto may depend on prior test steps (such as
  // extracting the AnnotatedPageContent, so that the provider can then find the
  // content node id from the APC). Prefer to use the wrappers like ClickAction,
  // NavigateAction, etc.
  MultiStep ExecuteAction(ActionProtoProvider proto_provider,
                          ExpectedErrorResult expected_result = {});

  MultiStep ExecuteInGlic(
      base::OnceCallback<void(content::WebContents*)> callback);

  MultiStep CreateTask(actor::TaskId& out_task, std::string_view title);

  // Note: In all the Create*Action functions below, parameters that are
  // expected to be created as a result of test steps (task_id, tab_handle,
  // etc.) are passed by reference since they'll be evaluated at time of use
  // (i.e. when running the test step). Passing by non-const ref prevents
  // binding an rvalue argument to these parameters since the test step won't be
  // executed until after these functions are invoked.
  MultiStep CreateTabAction(actor::TaskId& task_id,
                            SessionID window_id,
                            bool foreground,
                            ExpectedErrorResult expected_result = {});

  MultiStep GetClientRect(ui::ElementIdentifier tab_id,
                          std::string_view element_id,
                          gfx::Rect& out_rect);
  MultiStep ClickAction(
      std::string_view label,
      optimization_guide::proto::ClickAction::ClickType click_type,
      optimization_guide::proto::ClickAction::ClickCount click_count,
      actor::TaskId& task_id,
      tabs::TabHandle& tab_handle,
      ExpectedErrorResult expected_result = {});

  MultiStep ClickAction(
      std::string_view label,
      optimization_guide::proto::ClickAction::ClickType click_type,
      optimization_guide::proto::ClickAction::ClickCount click_count,
      ExpectedErrorResult expected_result = {});

  MultiStep ClickAction(
      const gfx::Point& coordinate,
      optimization_guide::proto::ClickAction::ClickType click_type,
      optimization_guide::proto::ClickAction::ClickCount click_count,
      actor::TaskId& task_id,
      tabs::TabHandle& tab_handle,
      ExpectedErrorResult expected_result = {});

  MultiStep ClickAction(
      const gfx::Point& coordinate,
      optimization_guide::proto::ClickAction::ClickType click_type,
      optimization_guide::proto::ClickAction::ClickCount click_count,
      ExpectedErrorResult expected_result = {});

  // The above functions take the coordinate as an rvalue which means the value
  // of the parameter is used at the time the step is declared, not when its
  // run. This function takes the coordinate by address to emphasise that the
  // step will use the value of the coordinate when the step runs, enabling
  // tests to click on a dynamically generated point (e.g. read an element's
  // bounding box and click on it).
  MultiStep ClickAction(
      const gfx::Point* coordinate,
      optimization_guide::proto::ClickAction::ClickType click_type,
      optimization_guide::proto::ClickAction::ClickCount click_count,
      ExpectedErrorResult expected_result = {});

  MultiStep NavigateAction(GURL url,
                           actor::TaskId& task_id,
                           tabs::TabHandle& tab_handle,
                           ExpectedErrorResult expected_result = {});

  MultiStep NavigateAction(GURL url, ExpectedErrorResult expected_result = {});

  // Starts a new task by executing an initial navigate action to `task_url` to
  // create a new tab. The new tab can then be referenced by the identifier
  // passed in `new_tab_id`. Stores the created task's id in `task_id_` and the
  // new tab's handle in `tab_handle_`.
  // If `open_in_foreground` is true (default), the new tab becomes active.
  // If false, the tab opens in the background, preventing the browser window
  // from stealing focus (useful for avoiding window-activation side effects in
  // tests).
  MultiStep StartActorTaskInNewTab(const GURL& task_url,
                                   ui::ElementIdentifier new_tab_id,
                                   bool open_in_foreground = true);

  // After invoking APIs that don't return promises, we round trip to both the
  // client and host to make sure the call has made it to the browser.
  MultiStep RoundTrip(actor::TaskId& task_id);

  // Stops a running task by calling the glic StopActorTask API.
  MultiStep StopActorTask();

  // Pauses a running task by calling the glic PauseActorTask API.
  MultiStep PauseActorTask();

  // Resumes a paused task by calling the glic ResumeActorTask API.
  MultiStep ResumeActorTask(base::Value::Dict context_options,
                            ExpectedResumeResult expected_result);

  // Interrupts a task by calling the glic InterruptActorTask API.
  MultiStep InterruptActorTask();

  MultiStep WaitForActorTaskState(mojom::ActorTaskState expected_state);

  // Gets a reference to a state observable for use in
  // WaitForActorTaskStateToStopped.
  MultiStep PrepareForStopStateChange();

  // Uses the state observable from PrepareForStopStateChange to await a state
  // change to stopped.
  MultiStep WaitForActorTaskStateChangeToStopped();

  // Foregrounds the last acted on tab by calling the glic
  // activateTab API.
  MultiStep ActivateTaskTab();

  // Waits for the glic getTabById for the task tab to satisfy the expected
  // foreground value.
  MultiStep WaitForTaskTabForeground(bool expected_foreground);

  // Returns a callback that returns the given string as the action proto. Meant
  // for testing error handling since this allows providing an invalid proto.
  ActionProtoProvider ArbitraryStringProvider(std::string_view str);

  // Gets the context options to capture a new observation after completing an
  // action. This includes both annotations (i.e. AnnotatedPageContent) and a
  // screenshot.
  base::Value::Dict UpdatedContextOptions();

  MultiStep InitializeWithOpenGlicWindow();

  // Triggers a page context fetch for the actor task tab and stores the
  // result in `annotated_page_content_`.
  MultiStep GetPageContextForActorTab();

  // Ensure whether a task is actively (non-paused) acting on the given tab.
  MultiStep CheckIsActingOnTab(ui::ElementIdentifier tab, bool expected);

  // Ensure whether a task is currently associated with the given tab, paused or
  // not.
  MultiStep CheckHasTaskForTab(ui::ElementIdentifier tab, bool expected);

  MultiStep CheckIsWebContentsCaptured(ui::ElementIdentifier tab,
                                       bool expected);

  const std::optional<optimization_guide::proto::ActionsResult>&
  last_execution_result() const;

  static std::string EncodeActionProto(
      const optimization_guide::proto::Actions& action);
  static std::optional<optimization_guide::proto::ActionsResult>
  DecodeActionsResultProto(const std::string& base64_proto);

  // The default task_id and tab created by StartActorTaskInNewTab. Most tests
  // will use these to act in the single tab of a task so these are stored for
  // convenience. More complicated tests involving multiple tasks or tabs will
  // have to manage their own handles/ids.
  actor::TaskId task_id_;
  tabs::TabHandle tab_handle_;

 protected:
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      annotated_page_content_;

  // Label corresponds to the aria-label on the element in the page.
  int32_t SearchAnnotatedPageContent(std::string_view label);

 private:
  std::optional<optimization_guide::proto::ActionsResult>
      last_execution_result_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_INTERACTIVE_UITEST_COMMON_H_
