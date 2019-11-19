// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/base_event_utils.h"

namespace autofill {

// AutofillManagerTestDelegateImpl --------------------------------------------
AutofillManagerTestDelegateImpl::AutofillManagerTestDelegateImpl()
    : is_expecting_dynamic_refill_(false) {}

AutofillManagerTestDelegateImpl::~AutofillManagerTestDelegateImpl() {}

void AutofillManagerTestDelegateImpl::DidPreviewFormData() {
  DCHECK(event_waiter_);
  event_waiter_->OnEvent(ObservedUiEvents::kPreviewFormData);
}

void AutofillManagerTestDelegateImpl::DidFillFormData() {
  DCHECK(event_waiter_);
  event_waiter_->OnEvent(ObservedUiEvents::kFormDataFilled);
}

void AutofillManagerTestDelegateImpl::DidShowSuggestions() {
  DCHECK(event_waiter_);
  event_waiter_->OnEvent(ObservedUiEvents::kSuggestionShown);
}

void AutofillManagerTestDelegateImpl::OnTextFieldChanged() {}

void AutofillManagerTestDelegateImpl::SetExpectations(
    std::list<ObservedUiEvents> expected_events,
    base::TimeDelta timeout) {
  event_waiter_ =
      std::make_unique<EventWaiter<ObservedUiEvents>>(expected_events, timeout);
}

bool AutofillManagerTestDelegateImpl::Wait() {
  return event_waiter_->Wait();
}

// AutofillUiTest ----------------------------------------------------
AutofillUiTest::AutofillUiTest()
    : key_press_event_sink_(
          base::BindRepeating(&AutofillUiTest::HandleKeyPressEvent,
                              base::Unretained(this))) {}

AutofillUiTest::~AutofillUiTest() {}

void AutofillUiTest::SetUpOnMainThread() {
  LOG(ERROR) << "crbug/967588: AutofillUiTest::SetUpOnMainThread() entered";
  // Don't want Keychain coming up on Mac.
  test::DisableSystemServices(browser()->profile()->GetPrefs());

  // Inject the test delegate into the AutofillManager of the main frame.
  RenderFrameHostChanged(/* old_host = */ nullptr,
                         /* new_host = */ GetWebContents()->GetMainFrame());
  Observe(GetWebContents());

  // If the mouse happened to be over where the suggestions are shown, then
  // the preview will show up and will fail the tests. We need to give it a
  // point that's within the browser frame, or else the method hangs.
  gfx::Point reset_mouse(GetWebContents()->GetContainerBounds().origin());
  reset_mouse = gfx::Point(reset_mouse.x() + 5, reset_mouse.y() + 5);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(reset_mouse));
  LOG(ERROR) << "crbug/967588: AutofillUiTest::SetUpOnMainThread() exited";
}

void AutofillUiTest::TearDownOnMainThread() {
  // Make sure to close any showing popups prior to tearing down the UI.
  AutofillManager* autofill_manager = GetAutofillManager();
  if (autofill_manager)
    autofill_manager->client()->HideAutofillPopup();
  test::ReenableSystemServices();
}

void AutofillUiTest::SendKeyToPage(content::WebContents* web_contents,
                                   const ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  content::SimulateKeyPress(web_contents, key, code, key_code, false, false,
                            false, false);
}

void AutofillUiTest::SendKeyToPageAndWait(
    ui::DomKey key,
    std::list<ObservedUiEvents> expected_events) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToPageAndWait(key, code, key_code, std::move(expected_events));
}

void AutofillUiTest::SendKeyToPageAndWait(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code,
    std::list<ObservedUiEvents> expected_events) {
  test_delegate()->SetExpectations(std::move(expected_events));
  content::SimulateKeyPress(GetWebContents(), key, code, key_code, false, false,
                            false, false);
  test_delegate()->Wait();
}

void AutofillUiTest::SendKeyToPopup(content::RenderFrameHost* render_frame_host,
                                    const ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  content::RenderWidgetHost* widget =
      render_frame_host->GetView()->GetRenderWidgetHost();

  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
}

void AutofillUiTest::SendKeyToPopupAndWait(
    ui::DomKey key,
    std::list<ObservedUiEvents> expected_events,
    content::RenderWidgetHost* widget) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToPopupAndWait(key, code, key_code, std::move(expected_events),
                        widget ? widget : GetRenderViewHost()->GetWidget());
}

void AutofillUiTest::SendKeyToPopupAndWait(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code,
    std::list<ObservedUiEvents> expected_events,
    content::RenderWidgetHost* widget) {
  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  test_delegate()->SetExpectations(std::move(expected_events));
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  test_delegate()->Wait();
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
}

void AutofillUiTest::DoNothingAndWait(unsigned seconds) {
  test_delegate()->SetExpectations({ObservedUiEvents::kNoEvent},
                                   base::TimeDelta::FromSeconds(seconds));
  ASSERT_FALSE(test_delegate()->Wait());
}

void AutofillUiTest::SendKeyToDataListPopup(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToDataListPopup(key, code, key_code);
}

// Datalist does not support autofill preview. There is no need to start
// message loop for Datalist.
void AutofillUiTest::SendKeyToDataListPopup(ui::DomKey key,
                                            ui::DomCode code,
                                            ui::KeyboardCode key_code) {
  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
      key_press_event_sink_);
  GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_event_sink_);
}

bool AutofillUiTest::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  return true;
}

content::WebContents* AutofillUiTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderViewHost* AutofillUiTest::GetRenderViewHost() {
  return GetWebContents()->GetRenderViewHost();
}

AutofillManager* AutofillUiTest::GetAutofillManager() {
  ContentAutofillDriver* driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(current_main_rfh_);
  // ContentAutofillDriver will be null if the current RenderFrameHost
  // is not owned by the current WebContents. This state appears to occur
  // when there is a web page popup during teardown
  if (!driver)
    return nullptr;
  return driver->autofill_manager();
}

void AutofillUiTest::RenderFrameHostChanged(
    content::RenderFrameHost* old_frame,
    content::RenderFrameHost* new_frame) {
  if (current_main_rfh_ != old_frame)
    return;
  current_main_rfh_ = new_frame;
  AutofillManager* autofill_manager = GetAutofillManager();
  if (autofill_manager)
    autofill_manager->SetTestDelegate(test_delegate());
}

}  // namespace autofill
