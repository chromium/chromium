// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os, ObservedUiEvents event) {
  switch (event) {
    case ObservedUiEvents::kPreviewFormData:
      return os << "kPreviewFormData";
    case ObservedUiEvents::kFormDataFilled:
      return os << "kFormDataFilled";
    case ObservedUiEvents::kSuggestionsShown:
      return os << "kSuggestionsShown";
    case ObservedUiEvents::kSuggestionsHidden:
      return os << "kSuggestionsHidden";
    case ObservedUiEvents::kNoEvent:
      return os << "kNoEvent";
    default:
      return os << "<OutOfRange>";
  }
}

// Keep in sync with BoundsOverlapWithAnyOpenPrompt() from
// autofill_popup_view_utils.cc.
void TryToCloseAllPrompts(content::WebContents* web_contents) {
  gfx::NativeView top_level_view =
      platform_util::GetViewForWindow(web_contents->GetTopLevelNativeWindow());
  DCHECK(top_level_view);

  // On Aura-based systems, prompts are siblings to the top level native window,
  // and hence we need to go one level up to start searching from the root
  // window.
  top_level_view = platform_util::GetParent(top_level_view)
                       ? platform_util::GetParent(top_level_view)
                       : top_level_view;
  views::Widget::Widgets all_widgets;
  views::Widget::GetAllChildWidgets(top_level_view, &all_widgets);
  for (views::Widget* w : all_widgets) {
    if (w->IsDialogBox())
      w->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

// BrowserAutofillManagerTestDelegateImpl
// --------------------------------------------
BrowserAutofillManagerTestDelegateImpl::
    BrowserAutofillManagerTestDelegateImpl() = default;

BrowserAutofillManagerTestDelegateImpl::
    ~BrowserAutofillManagerTestDelegateImpl() = default;

void BrowserAutofillManagerTestDelegateImpl::SetIgnoreBackToBackMessages(
    ObservedUiEvents type,
    bool ignore) {
  if (ignore) {
    ignore_back_to_back_event_types_.insert(type);
  } else {
    ignore_back_to_back_event_types_.erase(type);
    if (last_event_ == type)
      last_event_ = ObservedUiEvents::kNoEvent;
  }
}

void BrowserAutofillManagerTestDelegateImpl::FireEvent(ObservedUiEvents event) {
  if (event_waiter_ && (!ignore_back_to_back_event_types_.contains(event) ||
                        last_event_ != event)) {
    event_waiter_->OnEvent(event);
  }
  last_event_ = event;
}

void BrowserAutofillManagerTestDelegateImpl::DidPreviewFormData() {
  FireEvent(ObservedUiEvents::kPreviewFormData);
}

void BrowserAutofillManagerTestDelegateImpl::DidFillFormData() {
  FireEvent(ObservedUiEvents::kFormDataFilled);
}

void BrowserAutofillManagerTestDelegateImpl::DidShowSuggestions() {
  FireEvent(ObservedUiEvents::kSuggestionsShown);
}

void BrowserAutofillManagerTestDelegateImpl::DidHideSuggestions() {
  FireEvent(ObservedUiEvents::kSuggestionsHidden);
}

void BrowserAutofillManagerTestDelegateImpl::SetExpectations(
    std::list<ObservedUiEvents> expected_events,
    base::TimeDelta timeout,
    base::Location location) {
  event_waiter_ = std::make_unique<EventWaiter<ObservedUiEvents>>(
      expected_events, timeout, location);
}

testing::AssertionResult BrowserAutofillManagerTestDelegateImpl::Wait() {
  return event_waiter_->Wait();
}

// AutofillUiTest ----------------------------------------------------
AutofillUiTest::AutofillUiTest(
    const test::AutofillTestEnvironment::Options& options)
    : autofill_test_environment_(options) {}

AutofillUiTest::~AutofillUiTest() = default;

void AutofillUiTest::SetUpOnMainThread() {
  auto* client =
      ChromeAutofillClient::FromWebContentsForTesting(GetWebContents());

  // Make autofill popup stay open by ignoring external changes when possible.
  client->SetKeepPopupOpenForTesting(true);

  // Inject the test delegate into the BrowserAutofillManager of the main frame.
  RenderFrameHostChanged(
      /*old_host=*/nullptr,
      /*new_host=*/GetWebContents()->GetPrimaryMainFrame());
  Observe(GetWebContents());

  // Refills normally only happen if the form changes within 1 second of the
  // initial fill. On a slow bot, this may lead to flakiness. We hence set a
  // very high limit.
  test_api(test_api(*GetBrowserAutofillManager()).form_filler())
      .set_limit_before_refill(base::Hours(1));
  autofill_driver_factory_observation_.Observe(
      &client->GetAutofillDriverFactory());

  // Wait for Personal Data Manager to be fully loaded to prevent that
  // spurious notifications deceive the tests.
  WaitForPersonalDataManagerToBeLoaded(browser()->profile());

  disable_animation_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // If the mouse happened to be over where the suggestions are shown, then
  // the preview will show up and will fail the tests. We need to give it a
  // point that's within the browser frame, or else the method hangs.
  gfx::Point reset_mouse(GetWebContents()->GetContainerBounds().origin());
  reset_mouse = gfx::Point(reset_mouse.x() + 5, reset_mouse.y() + 5);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(reset_mouse));
}

void AutofillUiTest::TearDownOnMainThread() {
  // Make sure to close any showing popups prior to tearing down the UI.
  BrowserAutofillManager* autofill_manager = GetBrowserAutofillManager();
  if (autofill_manager)
    autofill_manager->client().HideAutofillSuggestions(
        SuggestionHidingReason::kTabGone);
  current_main_rfh_ = nullptr;
  InProcessBrowserTest::TearDownOnMainThread();
}

testing::AssertionResult AutofillUiTest::SendKeyToPageAndWait(
    ui::DomKey key,
    std::list<ObservedUiEvents> expected_events,
    base::TimeDelta timeout,
    base::Location location) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  return SendKeyToPageAndWait(key, code, key_code, std::move(expected_events),
                              timeout, location);
}

testing::AssertionResult AutofillUiTest::SendKeyToPageAndWait(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code,
    std::list<ObservedUiEvents> expected_events,
    base::TimeDelta timeout,
    base::Location location) {
  test_delegate()->SetExpectations(std::move(expected_events), timeout,
                                   location);
  content::SimulateKeyPress(GetWebContents(), key, code, key_code, false, false,
                            false, false);
  return test_delegate()->Wait();
}

void AutofillUiTest::SendKeyToPopup(content::RenderFrameHost* render_frame_host,
                                    const ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  content::RenderWidgetHost* widget =
      render_frame_host->GetView()->GetRenderWidgetHost();

  // Route popup-targeted key presses via the render view host.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
}

testing::AssertionResult AutofillUiTest::SendKeyToPopupAndWait(
    ui::DomKey key,
    std::list<ObservedUiEvents> expected_events,
    content::RenderWidgetHost* widget,
    base::TimeDelta timeout,
    base::Location location) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  return SendKeyToPopupAndWait(
      key, code, key_code, std::move(expected_events),
      widget ? widget : GetRenderViewHost()->GetWidget(), timeout, location);
}

testing::AssertionResult AutofillUiTest::SendKeyToPopupAndWait(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code,
    std::list<ObservedUiEvents> expected_events,
    content::RenderWidgetHost* widget,
    base::TimeDelta timeout,
    base::Location location) {
  // Route popup-targeted key presses via the render view host.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  test_delegate()->SetExpectations(std::move(expected_events), timeout,
                                   location);
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  testing::AssertionResult result = test_delegate()->Wait();
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
  return result;
}

void AutofillUiTest::DoNothingAndWait(base::TimeDelta timeout,
                                      base::Location location) {
  test_delegate()->SetExpectations({ObservedUiEvents::kNoEvent}, timeout,
                                   location);
  ASSERT_FALSE(test_delegate()->Wait());
}

void AutofillUiTest::DoNothingAndWaitAndIgnoreEvents(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

bool AutofillUiTest::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  return true;
}

content::WebContents* AutofillUiTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderViewHost* AutofillUiTest::GetRenderViewHost() {
  return GetWebContents()->GetPrimaryMainFrame()->GetRenderViewHost();
}

BrowserAutofillManager* AutofillUiTest::GetBrowserAutofillManager() {
  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(current_main_rfh_);
  // ContentAutofillDriver will be null if the current RenderFrameHost
  // is not owned by the current WebContents. This state appears to occur
  // when there is a web page popup during teardown
  if (!driver)
    return nullptr;
  return static_cast<BrowserAutofillManager*>(&driver->GetAutofillManager());
}

void AutofillUiTest::RenderFrameHostChanged(
    content::RenderFrameHost* old_frame,
    content::RenderFrameHost* new_frame) {
  if (current_main_rfh_ != old_frame)
    return;
  current_main_rfh_ = new_frame;
  if (BrowserAutofillManager* autofill_manager = GetBrowserAutofillManager()) {
    test_delegate()->Observe(*autofill_manager);
  }
}

void AutofillUiTest::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  autofill_driver_factory_observation_.Reset();
}

void AutofillUiTest::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  // Refills normally only happen if the form changes within 1 second of the
  // initial fill. On a slow bot, this may lead to flakiness. We hence set a
  // very high limit.
  test_api(test_api(static_cast<BrowserAutofillManager&>(
                        driver.GetAutofillManager()))
               .form_filler())
      .set_limit_before_refill(base::Hours(1));
}

}  // namespace autofill
