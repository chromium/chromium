// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_

#include <list>
#include <memory>
#include <ostream>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/dense_set.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

enum class ObservedUiEvents {
  kPreviewFormData,
  kFormDataFilled,
  kSuggestionsShown,
  kSuggestionsHidden,
  kNoEvent,
  kMaxValue = kNoEvent
};

std::ostream& operator<<(std::ostream& os, ObservedUiEvents event);

// Attempts to close all open bubbles.
// This is not reliable on Windows because on Windows bubbles are not
// necessarily children of the top-level view.
void TryToCloseAllPrompts(content::WebContents* web_contents);

class BrowserAutofillManagerTestDelegateImpl
    : public BrowserAutofillManagerTestDelegate {
 public:
  BrowserAutofillManagerTestDelegateImpl();

  BrowserAutofillManagerTestDelegateImpl(
      const BrowserAutofillManagerTestDelegateImpl&) = delete;
  BrowserAutofillManagerTestDelegateImpl& operator=(
      const BrowserAutofillManagerTestDelegateImpl&) = delete;

  ~BrowserAutofillManagerTestDelegateImpl() override;

  // Controls whether back-to-back events of |type|, except for the first one,
  // are ignored. This is useful for cross-iframe forms, where events such as
  // ObservedUiEvents::kFormDataFilled are triggered by each filled renderer
  // form.
  void SetIgnoreBackToBackMessages(ObservedUiEvents type, bool ignore);

  // autofill::BrowserAutofillManagerTestDelegate:
  void DidPreviewFormData() override;
  void DidFillFormData() override;
  void DidShowSuggestions() override;
  void DidHideSuggestions() override;

  void SetExpectations(std::list<ObservedUiEvents> expected_events,
                       base::TimeDelta timeout = base::Seconds(0),
                       base::Location location = FROM_HERE);
  [[nodiscard]] testing::AssertionResult Wait();

 private:
  void FireEvent(ObservedUiEvents event);

  std::unique_ptr<EventWaiter<ObservedUiEvents>> event_waiter_;
  DenseSet<ObservedUiEvents> ignore_back_to_back_event_types_;
  ObservedUiEvents last_event_ = ObservedUiEvents::kNoEvent;
};

class AutofillUiTest : public InProcessBrowserTest,
                       public content::WebContentsObserver,
                       public ContentAutofillDriverFactory::Observer {
 public:
  explicit AutofillUiTest(
      const test::AutofillTestEnvironment::Options& options = {
          .disable_server_communication = true});
  ~AutofillUiTest() override;

  AutofillUiTest(const AutofillUiTest&) = delete;
  AutofillUiTest& operator=(const AutofillUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  [[nodiscard]] testing::AssertionResult SendKeyToPageAndWait(
      ui::DomKey key,
      std::list<ObservedUiEvents> expected_events,
      base::TimeDelta timeout = {},
      base::Location location = FROM_HERE);
  [[nodiscard]] testing::AssertionResult SendKeyToPageAndWait(
      ui::DomKey key,
      ui::DomCode code,
      ui::KeyboardCode key_code,
      std::list<ObservedUiEvents> expected_events,
      base::TimeDelta timeout = {},
      base::Location location = FROM_HERE);

  void SendKeyToPopup(content::RenderFrameHost* render_frame_host,
                      const ui::DomKey key);
  // Send key to the render host view's widget if |widget| is null.
  [[nodiscard]] testing::AssertionResult SendKeyToPopupAndWait(
      ui::DomKey key,
      std::list<ObservedUiEvents> expected_events,
      content::RenderWidgetHost* widget = nullptr,
      base::TimeDelta timeout = {},
      base::Location location = FROM_HERE);
  [[nodiscard]] testing::AssertionResult SendKeyToPopupAndWait(
      ui::DomKey key,
      ui::DomCode code,
      ui::KeyboardCode key_code,
      std::list<ObservedUiEvents> expected_events,
      content::RenderWidgetHost* widget,
      base::TimeDelta timeout = {},
      base::Location location = FROM_HERE);

  void SendKeyToDataListPopup(ui::DomKey key);
  void SendKeyToDataListPopup(ui::DomKey key,
                              ui::DomCode code,
                              ui::KeyboardCode key_code);

  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event);

  // DoNothingAndWait() violates an assertion if during the time an event
  // happens. Delayed events during DoNothingAndWait() may therefore cause
  // flakiness. DoNothingAndWaitAndIgnoreEvents() ignores any events.
  void DoNothingAndWait(base::TimeDelta timeout,
                        base::Location location = FROM_HERE);
  void DoNothingAndWaitAndIgnoreEvents(base::TimeDelta timeout);

  content::WebContents* GetWebContents();
  content::RenderViewHost* GetRenderViewHost();
  BrowserAutofillManager* GetBrowserAutofillManager();

  BrowserAutofillManagerTestDelegateImpl* test_delegate() {
    return &test_delegate_;
  }
  content::RenderWidgetHost::KeyPressEventCallback key_press_GetEventSink();

 private:
  // WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // ContentAutofillDriverFactory::Observer
  void OnContentAutofillDriverFactoryDestroyed(
      ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(ContentAutofillDriverFactory& factory,
                                      ContentAutofillDriver& driver) override;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::ScopedObservation<ContentAutofillDriverFactory,
                          ContentAutofillDriverFactory::Observer>
      autofill_driver_factory_observation_{this};

  raw_ptr<content::RenderFrameHost> current_main_rfh_ = nullptr;
  BrowserAutofillManagerTestDelegateImpl test_delegate_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sink because if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_{
      base::BindRepeating(&AutofillUiTest::HandleKeyPressEvent,
                          base::Unretained(this))};

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animation_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
