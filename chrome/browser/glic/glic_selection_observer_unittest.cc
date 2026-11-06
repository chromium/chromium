// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/selection/quick_answers_tool.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/glic_enums.mojom-shared.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/quick_answers_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;

class TestGlicSelectionObserver : public GlicSelectionObserver {
 public:
  explicit TestGlicSelectionObserver(content::WebContents* web_contents)
      : GlicSelectionObserver(web_contents) {}

  using GlicSelectionObserver::PrimaryMainFrameWasResized;

  void UpdateSelectionState(const std::u16string& text,
                            bool is_pending_selection,
                            SelectionSource source) override {
    last_processed_text_ = text;
    update_count_++;
    if (call_base_update_selection_state_) {
      GlicSelectionObserver::UpdateSelectionState(text, is_pending_selection,
                                                  source);
    }
  }

  void DismissUI(DismissReason reason) override {
    dismiss_ui_called_ = true;
    dismiss_ui_reason_ = reason;
    GlicSelectionObserver::DismissUI(reason);
  }

  const std::optional<std::u16string>& last_processed_text() const {
    return last_processed_text_;
  }

  int update_count() const { return update_count_; }

  bool dismiss_ui_called() const { return dismiss_ui_called_; }
  std::optional<DismissReason> dismiss_ui_reason() const {
    return dismiss_ui_reason_;
  }

  void Reset() {
    last_processed_text_.reset();
    update_count_ = 0;
    dismiss_ui_called_ = false;
    dismiss_ui_reason_ = std::nullopt;
    call_base_update_selection_state_ = false;
    mock_panel_showing_ = false;
    send_context_called_ = false;
    last_sent_context_.reset();
    show_selection_affordance_called_ = false;
    last_affordance_text_.reset();
    mock_side_panel_open_ = true;
  }

  // Expose methods for testing.
  using GlicSelectionObserver::IsSidePanelOpen;
  using GlicSelectionObserver::OnInputEvent;
  using GlicSelectionObserver::OnPageContextEligibilityChanged;
  using GlicSelectionObserver::RenderFrameCreated;
  using GlicSelectionObserver::RenderFrameDeleted;

  void set_call_base_update_selection_state(bool value) {
    call_base_update_selection_state_ = value;
  }

  void set_mock_panel_showing(bool value) { mock_panel_showing_ = value; }
  bool BaseIsSidePanelOpen() const {
    return GlicSelectionObserver::IsSidePanelOpen();
  }
  void BaseSendAdditionalContextToPanel(tabs::TabInterface* tab_interface,
                                        const std::u16string& selected_text) {
    GlicSelectionObserver::SendAdditionalContextToPanel(tab_interface,
                                                        selected_text);
  }

  bool send_context_called() const { return send_context_called_; }
  const std::optional<std::u16string>& last_sent_context() const {
    return last_sent_context_;
  }

  bool show_selection_affordance_called() const {
    return show_selection_affordance_called_;
  }
  const std::optional<std::u16string>& last_affordance_text() const {
    return last_affordance_text_;
  }

 protected:
  bool IsTextSelectionSharingEnabled() const override { return true; }

  // Mirrors production: the inline cue is controlled by the feature.
  bool IsInlineCueEnabled() const override {
    return base::FeatureList::IsEnabled(features::kGlicSelectionPrompt);
  }

  bool IsPanelShowing(tabs::TabInterface* tab_interface,
                      BrowserWindowInterface* bwi) override {
    return mock_panel_showing_;
  }

  void SendAdditionalContextToPanel(
      tabs::TabInterface* tab_interface,
      const std::u16string& selected_text) override {
    if (!IsPageContextEligible() && !selected_text.empty()) {
      return;
    }
    last_sent_context_ = selected_text;
    send_context_called_ = true;
  }

  void ShowSelectionAffordance(const std::u16string& selected_text) override {
    show_selection_affordance_called_ = true;
    last_affordance_text_ = selected_text;
  }

  bool IsSidePanelOpen() const override { return mock_side_panel_open_; }

 private:
  std::optional<std::u16string> last_processed_text_;
  int update_count_ = 0;
  bool dismiss_ui_called_ = false;
  std::optional<DismissReason> dismiss_ui_reason_;

  bool call_base_update_selection_state_ = false;
  bool mock_panel_showing_ = false;
  bool mock_side_panel_open_ = true;
  bool send_context_called_ = false;
  std::optional<std::u16string> last_sent_context_;
  bool show_selection_affordance_called_ = false;
  std::optional<std::u16string> last_affordance_text_;
};

}  // namespace

namespace {
bool g_mock_eligibility = true;
bool MockIsEligibleWithAccount(
    const std::string&,
    const std::string&,
    const std::string&,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return g_mock_eligibility;
}
optimization_guide::StringViewSpan MockGetMeta(
    std::string_view,
    std::string_view,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return optimization_guide::StringViewSpan{.data = nullptr, .size = 0};
}
optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>&) {
  return optimization_guide::PageEligibilityResult{
      .status = optimization_guide::PageEligibility::kEligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}
optimization_guide::PageContextEligibilityAPI g_test_api = {
    .IsPageContextEligible = nullptr,
    .IsPageContextEligibleWithAccount = &MockIsEligibleWithAccount,
    .ShouldReextractPageContext = nullptr,
    .GetMetaTagNamesAffectingEligibility = &MockGetMeta,
    .CheckPageEligibility = &MockCheckPageEligibility,
};
}  // namespace

class GlicSelectionObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicSelectionObserverTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_eligibility_holder_ =
        std::make_unique<optimization_guide::PageContextEligibility>(
            &g_test_api);
    optimization_guide::PageContextEligibility::SetForTesting(
        test_eligibility_holder_.get());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    // Create our test observer.
    observer_ = std::make_unique<TestGlicSelectionObserver>(web_contents());
    task_environment()->RunUntilIdle();
  }

  void TearDown() override {
    observer_.reset();
    identity_test_env_adaptor_.reset();
    optimization_guide::PageContextEligibility::SetForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void SetMockEligibility(bool eligible) {
    g_mock_eligibility = eligible;
    NavigateAndCommit(GURL("https://example.com/"));
  }

  // To simulate creating the observer AFTER mock identity state is set.
  void RecreateObserver() {
    observer_ = std::make_unique<TestGlicSelectionObserver>(web_contents());
    task_environment()->RunUntilIdle();
  }

 protected:
  TestGlicSelectionObserver* GetObserver() { return observer_.get(); }

  content::RenderWidgetHost* GetRenderWidgetHost() {
    return web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

  size_t GetObservedFramesCount() const {
    return observer_->observed_frames_.size();
  }

  bool IsSelecting() const { return observer_->is_selecting_; }

  void SimulateMouseMove(float x, float y) {
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.SetPositionInWidget(x, y);
    observer_->OnInputEvent(*GetRenderWidgetHost(), event,
                            content::RenderWidgetHost::InputEventObserver::
                                InputEventSource::kUnknown);
    task_environment()->RunUntilIdle();
  }

  void SimulateMouseShake() {
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
  }

  std::unique_ptr<TestGlicSelectionObserver> observer_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<optimization_guide::PageContextEligibility>
      test_eligibility_holder_;
};

TEST_F(GlicSelectionObserverTest, ObserverDeduplicatesRenderWidgetHosts) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  NavigateAndCommit(GURL("http://example.com"));
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();

  // Create a child frame. It should share the RenderWidgetHost with the main
  // frame.
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("child");
  ASSERT_TRUE(child_rfh);
  ASSERT_EQ(main_rfh->GetRenderWidgetHost(), child_rfh->GetRenderWidgetHost());

  // Trigger RenderFrameCreated for both.
  observer->RenderFrameCreated(main_rfh);
  observer->RenderFrameCreated(child_rfh);

  EXPECT_EQ(2u, GetObservedFramesCount());

  // Removing the child frame should remove it from the map, but not the main
  // frame.
  observer->RenderFrameDeleted(child_rfh);
  EXPECT_EQ(1u, GetObservedFramesCount());

  // We can't directly check the internal observer list of RenderWidgetHost
  // without exposing it in test headers, but we can verify our observer's map
  // handles the duplicate RenderWidgetHost correctly.

  observer->RenderFrameDeleted(main_rfh);
  EXPECT_EQ(0u, GetObservedFramesCount());
}

TEST_F(GlicSelectionObserverTest, SelectionClearsInstantly) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Set initial selection (processed instantly when not selecting).
  observer->OnTextSelectionChanged(nullptr, u"Initial");
  EXPECT_EQ(1, observer->update_count());
  observer->Reset();

  // Clear selection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // Clearing is also processed instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, TooLongSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMaxSelectionLength is 1000.
  std::u16string huge_text(1001, 'a');
  observer->OnTextSelectionChanged(nullptr, huge_text);

  // Should be treated as clearing (empty text) instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, TooShortSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMinSelectionLength is 3.
  std::u16string short_text(2, 'a');
  observer->OnTextSelectionChanged(nullptr, short_text);

  // Should be treated as clearing (empty text) instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, WhitespaceIgnoredInLengthCheck) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "a b " has 2 non-whitespace characters.
  std::u16string text = u"a b ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be treated as clearing (empty text) because it has < 3
  // non-whitespace chars.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // "a b c" has 3 non-whitespace characters.
  text = u"a b c";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(text, *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionTrimmed) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "  abc  " should be trimmed to "abc".
  std::u16string text = u"  abc  ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted and trimmed.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"abc", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, MultipleSelectionUpdatesDuringDrag) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate MouseDown to start drag.
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // First selection update during drag.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(0, observer->update_count());

  // Second selection update during drag.
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(0, observer->update_count());

  // Simulate MouseUp to complete drag.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify that only the last selection is processed instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Second", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, KeyboardSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Simulate a keyboard event.
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  // Simulate a keyboard event using the WebContents RenderWidgetHost.
  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing a collapsed selection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // It should clear the selection immediately.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // Now simulate a mouse down event to clear the keyboard state.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Mouse Selection");
  // Should be ignored during selection drag.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a MouseUp event.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Should be processed instantly on MouseUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Mouse Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, InlineKeyboardSelectionByShiftAndArrowKeys) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate a Shift and Arrow Right keydown event.
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_RIGHT;

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing the expanding selection.
  observer->OnTextSelectionChanged(nullptr, u"Keyboard Selection");

  // The selection update should be deferred while actively selecting.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a KeyUp event to complete the inline selection.
  blink::WebKeyboardEvent key_up(
      blink::WebInputEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // The final selection should be processed instantly on KeyUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Keyboard Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectAllCommandDebouncedProperly) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate a Ctrl+A keydown event.
#if BUILDFLAG(IS_MAC)
  int modifier = blink::WebInputEvent::Modifiers::kMetaKey;
#else
  int modifier = blink::WebInputEvent::Modifiers::kControlKey;
#endif
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, modifier,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_A;

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing the entire selected page.
  observer->OnTextSelectionChanged(nullptr, u"Entire Page Selected Text");

  // The UI update should be deferred mid-selection command execution.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a KeyUp event to finalize command evaluation.
  blink::WebKeyboardEvent key_up(
      blink::WebInputEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // The selected page text should be processed instantly on KeyUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Entire Page Selected Text", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, InputEventsDismissUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  // Keyboard events should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(nullptr));
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Mouse clicks should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(nullptr));
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Scroll events should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface()).Times(0);
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, scroll_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();
}

TEST_F(GlicSelectionObserverTest, PrimaryMainFrameResizedDismissesUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  const gfx::Size current_size = web_contents()->GetSize();
  web_contents()->Resize(
      gfx::Rect(current_size.width() + 10, current_size.height() + 10));
  observer->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
}

TEST_F(GlicSelectionObserverTest,
       PrimaryMainFrameResizeIgnoredWhenSizeUnchanged) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->PrimaryMainFrameWasResized(/*width_changed=*/false);
  EXPECT_FALSE(observer->dismiss_ui_called());
}

TEST_F(GlicSelectionObserverTest,
       PrimaryMainFrameResizedAfterSelectionDuringPendingResizeDismissesUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Resize WebContents before PrimaryMainFrameWasResized fires (e.g. when a
  // visual properties ack is pending), then select text before the resize
  // notification arrives.
  const gfx::Size current_size = web_contents()->GetSize();
  web_contents()->Resize(
      gfx::Rect(current_size.width() + 10, current_size.height() + 10));
  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  ASSERT_FALSE(observer->dismiss_ui_called());

  observer->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnlyAfterMouseUp) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate MouseDown.
  blink::WebMouseEvent mouse_down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Selection changes during drag.
  observer->OnTextSelectionChanged(nullptr, u"Drag Selection");

  // Verify no update has been processed yet.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(0, observer->update_count());

  // Simulate MouseUp.
  blink::WebMouseEvent mouse_up_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify the update has been processed immediately.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Drag Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnTripleClick) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate one click.
  blink::WebMouseEvent mouse_down1(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down1.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down1.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_down1,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  blink::WebMouseEvent mouse_up1(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up1.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up1.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_up1,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Simulate a double click.
  blink::WebMouseEvent mouse_down2(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down2.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down2.click_count = 2;
  observer->OnInputEvent(*rwh, mouse_down2,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Word");

  blink::WebMouseEvent mouse_up2(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up2.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up2.click_count = 2;
  observer->OnInputEvent(*rwh, mouse_up2,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for Word.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Word", *observer->last_processed_text());

  observer->Reset();

  // Simulate a triple click.
  blink::WebMouseEvent mouse_down3(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down3.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down3.click_count = 3;
  observer->OnInputEvent(*rwh, mouse_down3,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Entire Paragraph");

  blink::WebMouseEvent mouse_up3(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up3.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up3.click_count = 3;
  observer->OnInputEvent(*rwh, mouse_up3,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for Entire Paragraph.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Entire Paragraph", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnShiftClick) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Initial mouse selection.
  observer->OnTextSelectionChanged(nullptr, u"Initial Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Initial Text", *observer->last_processed_text());
  observer->Reset();

  // Hold Shift (KeyDown).
  blink::WebKeyboardEvent shift_down(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, shift_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Expect UI to be dismissed.
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  observer->Reset();

  // Simulate MouseDown with Shift modifier.
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Selection extended.
  observer->OnTextSelectionChanged(nullptr, u"Initial Text Extended");

  // Simulate MouseUp.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for extended text.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Initial Text Extended", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, UpdateSelectionStatePanelShowing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Selected Text", *observer->last_affordance_text());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

// Only normal browser windows show the widget. The panel still gets the
// selection.
TEST_F(GlicSelectionObserverTest, UpdateSelectionStateNoWidgetInPopup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  EXPECT_CALL(mock_bwi, GetType())
      .WillRepeatedly(Return(BrowserWindowInterface::TYPE_POPUP));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->show_selection_affordance_called());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

// Sharing the text selection with the panel does not depend on the inline cue
// feature, which only controls the selection affordance.
TEST_F(GlicSelectionObserverTest,
       UpdateSelectionStatePanelShowingWithoutSelectionPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->show_selection_affordance_called());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       UpdateSelectionStatePanelNotShowingGlobalShowHide) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(false);

  // Call UpdateSelectionState directly with is_pending_selection = false,
  // simulating OnGlobalPanelShowHide when the panel is not opened.
  observer->UpdateSelectionState(
      u"Selected Text",
      /*is_pending_selection=*/false,
      GlicSelectionObserver::SelectionSource::kAutomatic);

  EXPECT_FALSE(observer->show_selection_affordance_called());
  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, EligibleSelection) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Eligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Eligible Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest, IneligibleSelection) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Ineligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, DynamicEligibilityChangeClearsContext) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Eligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Eligible Text", *observer->last_sent_context());

  // Simulate eligibility changing to false dynamically.
  observer->Reset();
  SetMockEligibility(false);

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangePushesContextWhenPanelShowing) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  // Selection happens while ineligible and panel is showing.
  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  // Eligibility changes to eligible with panel open: context is sent.
  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  // We have to reset the selected text here because setting mock eligibility
  // causes a navigation which clears the previous selected text.
  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangeDoesNotPushContextWhenPanelClosed) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  // Selection happens while ineligible and panel is closed.
  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(false);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  // Eligibility changes to eligible with panel closed: context is NOT sent.
  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangeDoesNotPushContextWhenNullBrowserWindow) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(nullptr));

  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, IdentityManagerIntegration) {
  RecreateObserver();

  // Actually, we can test that the observer's initialization succeeds,
  // and we could potentially check if MockIsEligibleWithAccount receives the
  // right string if we used a more complex mock, but here we just ensure it
  // doesn't crash and returns gracefully when unsigned.
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);
  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  observer->OnTextSelectionChanged(nullptr, u"Test text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Test text", *observer->last_sent_context());

  // Signed in case
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  RecreateObserver();
  observer = GetObserver();
  ASSERT_TRUE(observer);
  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Test text signed in");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Test text signed in", *observer->last_sent_context());
  observer_.reset();
}

TEST_F(GlicSelectionObserverTest, BaseIsSidePanelOpenReturnsFalseWithoutTab) {
  EXPECT_FALSE(observer_->BaseIsSidePanelOpen());
}

class GlicSelectionObserverPromptTest : public GlicSelectionObserverTest {
 public:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/true);
    GlicSelectionObserverTest::SetUp();
    observer_.reset();

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&GlicSelectionObserverPromptTest::CreateService,
                            base::Unretained(this)));

    GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
    RecreateObserver();
  }

  void TearDown() override {
    observer_.reset();
    mock_service_ = nullptr;
    GlicSelectionObserverTest::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<NiceMock<MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_service_ = service.get();
    return service;
  }

  MockGlicKeyedService* mock_glic_service() { return mock_service_; }

 protected:
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
};

// Without an instance for the tab, the selection is delivered by an invocation
// that opts out of live mode rather than pulling a live conversation into the
// tab's side panel.
TEST_F(GlicSelectionObserverPromptTest,
       SendAdditionalContextToPanelInvokesWithoutInstance) {
  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  SetMockEligibility(true);

  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*mock_glic_service(),
              Invoke(Field(
                  &GlicInvokeOptions::target,
                  Field(&Target::live_mode_behavior, LiveModeBehavior::kFail))))
      .Times(1);

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab,
                                                  u"Sample selected text");
}

// An instance that exists but isn't showing also goes through an invocation,
// so the surface is resolved and shown.
TEST_F(GlicSelectionObserverPromptTest,
       SendAdditionalContextToPanelInvokesWhenInstanceHidden) {
  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  SetMockEligibility(true);

  NiceMock<MockGlicInstance> instance;
  EXPECT_CALL(instance, IsShowing()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillRepeatedly(Return(&instance));

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_glic_service(),
              Invoke(Field(
                  &GlicInvokeOptions::target,
                  Field(&Target::live_mode_behavior, LiveModeBehavior::kFail))))
      .Times(1);

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab,
                                                  u"Sample selected text");
}

// The common case: the panel is already showing for the tab. The selection is
// a background context update, so it goes straight to the instance and no
// invocation is started (an invocation would be rejected with
// kInvokeInProgress whenever another one is in flight, and would be recorded
// as a user journey).
TEST_F(GlicSelectionObserverPromptTest,
       SendAdditionalContextToPanelUsesInstanceWhenShowing) {
  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  SetMockEligibility(true);

  NiceMock<MockGlicInstance> instance;
  EXPECT_CALL(instance, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance, IsLiveMode()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillRepeatedly(Return(&instance));

  EXPECT_CALL(*mock_glic_service(), Invoke).Times(0);
  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab,
                                                  u"Sample selected text");

  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->source, mojom::AdditionalContextSource::kTextSelection);
  ASSERT_EQ(sent->parts.size(), 1u);
  ASSERT_TRUE(sent->parts[0]->is_data());
  EXPECT_EQ(sent->parts[0]->get_data()->mime_type,
            "application/x-glic-selection");

  // Clearing the selection takes the same path, with no parts.
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab, u"");
  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->source, mojom::AdditionalContextSource::kTextSelection);
  EXPECT_TRUE(sent->parts.empty());
}

// A showing live-mode conversation is left alone, matching what
// LiveModeBehavior::kFail does on the invoke path.
TEST_F(GlicSelectionObserverPromptTest,
       SendAdditionalContextToPanelSkipsShowingLiveModeInstance) {
  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  SetMockEligibility(true);

  NiceMock<MockGlicInstance> instance;
  EXPECT_CALL(instance, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance, IsLiveMode()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillRepeatedly(Return(&instance));

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_glic_service(), Invoke).Times(0);

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab,
                                                  u"Sample selected text");
}

// Clearing the selection when the tab has no showing instance must not trigger
// an invocation to open the panel.
TEST_F(
    GlicSelectionObserverPromptTest,
    SendAdditionalContextToPanelDoesNotInvokeWhenClearingWithoutShowingInstance) {
  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));
  SetMockEligibility(true);

  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_glic_service(), Invoke).Times(0);

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab, u"");

  NiceMock<MockGlicInstance> hidden_instance;
  EXPECT_CALL(hidden_instance, IsShowing()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_glic_service(), GetInstanceForTab(&mock_tab))
      .WillOnce(Return(&hidden_instance));
  EXPECT_CALL(hidden_instance, SendAdditionalContext).Times(0);

  GetObserver()->BaseSendAdditionalContextToPanel(&mock_tab, u"");
}

TEST_F(GlicSelectionObserverPromptTest, ShakeInvokesRegionCapture) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicShakeTrigger);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  EXPECT_CALL(
      *mock_glic_service(),
      Invoke(AllOf(Property(&GlicInvokeOptions::GetInvocationSource,
                            mojom::InvocationSource::kCaptureRegionHotkey),
                   Field(&GlicInvokeOptions::wait_for_panel_open, true))))
      .Times(1);

  SimulateMouseShake();
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnClickAway) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Set selection context sent (e.g. from context menu invocation in editable
  // or non-editable field).
  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate left click down on the page (clicking away).
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !observer->has_sent_selection_context(); }));

  // Verify that the selection context has been cleared and empty context sent.
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnTextSelectionChanged) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate keyboard/programmatic deselection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // Verify that the selection context has been cleared and empty context sent.
  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnUnshareablePageDeselection) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  // Navigate to an unshareable chrome:// URL.
  NavigateAndCommit(GURL("chrome://version"));

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate deselection on unshareable page.
  observer->OnTextSelectionChanged(nullptr, u"");

  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnPrimaryPageChanged) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Navigate to a new primary page.
  NavigateAndCommit(GURL("https://example.com/page2"));

  // Verify that navigating cleared the selection context and notified the
  // panel.
  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest, DismissUIClearsPendingSelection) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate MouseDown to start drag.
  blink::WebMouseEvent mouse_down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  ASSERT_TRUE(base::test::RunUntil([&]() { return IsSelecting(); }));

  // Selection changes during drag.
  observer->OnTextSelectionChanged(nullptr, u"Drag Selection");
  EXPECT_EQ(0, observer->update_count());

  // Dismiss UI (e.g. when a context menu opens).
  observer->DismissUI(GlicSelectionObserver::DismissReason::kExternal);

  // Simulate MouseUp. The pending selection should have been cleared.
  blink::WebMouseEvent mouse_up_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !IsSelecting(); }));

  EXPECT_EQ(0, observer->update_count());
}

namespace {

optimization_guide::OptimizationMetadata MakeQuickAnswersMetadata(
    bool is_eligible) {
  optimization_guide::proto::QuickAnswersMetadata quick_answers_metadata;
  quick_answers_metadata.set_is_eligible(is_eligible);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(
      optimization_guide::AnyWrapProto(quick_answers_metadata));
  return metadata;
}

}  // namespace

class GlicSelectionObserverQuickAnswersTest : public GlicSelectionObserverTest {
 public:
  explicit GlicSelectionObserverQuickAnswersTest(
      bool enable_quick_answers_feature = true,
      bool create_optimization_guide_service = true)
      : enable_quick_answers_feature_(enable_quick_answers_feature),
        create_optimization_guide_service_(create_optimization_guide_service) {
    if (enable_quick_answers_feature_) {
      quick_answers_feature_list_.InitWithFeatures(
          {features::kGlicSelectionPrompt,
           features::kQuickAnswersSelectionSuggestions},
          {});
    } else {
      quick_answers_feature_list_.InitWithFeatures(
          {features::kGlicSelectionPrompt},
          {features::kQuickAnswersSelectionSuggestions});
    }
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    auto factories = GlicSelectionObserverTest::GetTestingFactories();
    factories.emplace_back(
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            [](bool enable_quick_answers_feature,
               bool create_optimization_guide_service,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              if (!create_optimization_guide_service) {
                return nullptr;
              }
              auto service = std::make_unique<
                  NiceMock<MockOptimizationGuideKeyedService>>();
              EXPECT_CALL(*service, RegisterOptimizationTypes(_))
                  .Times(AnyNumber());
              EXPECT_CALL(*service, RegisterOptimizationTypes(ElementsAre(
                                        optimization_guide::proto::
                                            OptimizationType::QUICK_ANSWERS)))
                  .Times(enable_quick_answers_feature ? 1 : 0);
              return service;
            },
            enable_quick_answers_feature_, create_optimization_guide_service_));
    return factories;
  }

  void SetUp() override {
    GlicSelectionObserverTest::SetUp();

    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_bwi_));
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_);
    if (enable_quick_answers_feature_) {
      quick_answers_tool_ = std::make_unique<QuickAnswersTool>(mock_tab_);
    }
    RecreateObserver();

    if (create_optimization_guide_service_) {
      ASSERT_TRUE(mock_optimization_guide());
      Mock::VerifyAndClearExpectations(mock_optimization_guide());
    } else {
      EXPECT_FALSE(mock_optimization_guide());
    }
  }

  void TearDown() override {
    observer_.reset();
    quick_answers_tool_.reset();
    GlicSelectionObserverTest::TearDown();
  }

 protected:
  MockOptimizationGuideKeyedService* mock_optimization_guide() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

  void RecreateQuickAnswersToolAndObserver() {
    observer_.reset();
    quick_answers_tool_.reset();
    if (enable_quick_answers_feature_) {
      quick_answers_tool_ = std::make_unique<QuickAnswersTool>(mock_tab_);
    }
    RecreateObserver();
  }

 private:
  bool enable_quick_answers_feature_;
  bool create_optimization_guide_service_;
  base::test::ScopedFeatureList quick_answers_feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_bwi_;
  std::unique_ptr<QuickAnswersTool> quick_answers_tool_;
};

class GlicSelectionObserverQuickAnswersDisabledTest
    : public GlicSelectionObserverQuickAnswersTest {
 public:
  GlicSelectionObserverQuickAnswersDisabledTest()
      : GlicSelectionObserverQuickAnswersTest(
            /*enable_quick_answers_feature=*/false) {}
};

class GlicSelectionObserverQuickAnswersNoServiceTest
    : public GlicSelectionObserverQuickAnswersTest {
 public:
  GlicSelectionObserverQuickAnswersNoServiceTest()
      : GlicSelectionObserverQuickAnswersTest(
            /*enable_quick_answers_feature=*/true,
            /*create_optimization_guide_service=*/false) {}
};

TEST_F(GlicSelectionObserverQuickAnswersDisabledTest,
       DoesNotQueryOptimizationGuideWhenFeatureDisabled) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  EXPECT_CALL(*mock_optimization_guide(),
              CanApplyOptimization(
                  _, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  NavigateAndCommit(GURL("https://example.com/page1"));

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Selected Text", *observer->last_affordance_text());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       ShowsInlineCueWhenQuickAnswersEligible) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  const GURL url("https://example.com/eligible");
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/true)));

  NavigateAndCommit(url);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Selected Text", *observer->last_affordance_text());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       SuppressesInlineCueWhenQuickAnswersIneligibleButSharesWithPanel) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  const GURL url("https://example.com/ineligible");
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/false)));

  NavigateAndCommit(url);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->show_selection_affordance_called());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       SuppressesInlineCueWhenDecisionIsFalseUnknownOrMissingMetadata) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  // Case 1: kFalse -> fail closed.
  const GURL url_false("https://example.com/false");
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url_false, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          optimization_guide::OptimizationMetadata()));
  NavigateAndCommit(url_false);
  observer->OnTextSelectionChanged(nullptr, u"Text False");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());
  observer->Reset();
  observer->set_call_base_update_selection_state(true);

  // Case 2: kUnknown -> fail closed.
  const GURL url_unknown("https://example.com/unknown");
  EXPECT_CALL(*mock_optimization_guide(),
              CanApplyOptimization(
                  url_unknown,
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kUnknown,
          optimization_guide::OptimizationMetadata()));
  NavigateAndCommit(url_unknown);
  observer->OnTextSelectionChanged(nullptr, u"Text Unknown");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());
  observer->Reset();
  observer->set_call_base_update_selection_state(true);

  // Case 3: kTrue with absent QuickAnswersMetadata -> fail closed.
  const GURL url_missing_meta("https://example.com/missing-meta");
  EXPECT_CALL(*mock_optimization_guide(),
              CanApplyOptimization(
                  url_missing_meta,
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          optimization_guide::OptimizationMetadata()));
  NavigateAndCommit(url_missing_meta);
  observer->OnTextSelectionChanged(nullptr, u"Text Missing Meta");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());
}

TEST_F(GlicSelectionObserverQuickAnswersNoServiceTest,
       SuppressesInlineCueWhenOptimizationGuideServiceUnavailable) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  // Before any navigation commits -> fail closed.
  observer->OnTextSelectionChanged(nullptr, u"Before Nav");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());

  // After navigation commits without an OptimizationGuideKeyedService -> fail
  // closed.
  NavigateAndCommit(GURL("https://example.com/no-service"));
  observer->OnTextSelectionChanged(nullptr, u"After Nav");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       SuppressesInlineCueWhileDecisionInFlightAndDoesNotShowWhenResolved) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  const GURL url("https://example.com/inflight-eligible");
  optimization_guide::OptimizationGuideDecisionCallback saved_callback;
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&saved_callback));

  NavigateAndCommit(url);
  ASSERT_TRUE(saved_callback);

  // Select text while decision is in-flight: cue should not be shown.
  observer->OnTextSelectionChanged(nullptr, u"Inflight Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());

  // Resolve decision as eligible: cue is not retroactively shown for the prior
  // selection, but subsequent selections on the page do show it.
  std::move(saved_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/true));
  EXPECT_FALSE(observer->show_selection_affordance_called());

  observer->OnTextSelectionChanged(nullptr, u"Subsequent Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Subsequent Text", *observer->last_affordance_text());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       SkipsFragmentNavigationAndRequeriesOnSameDocNavigation) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  const GURL url1("https://example.com/page1");
  const GURL url2("https://example.com/page2");

  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url1, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/true)));

  NavigateAndCommit(url1);

  // Fragment-only same-document navigation should NOT trigger a new query.
  EXPECT_CALL(*mock_optimization_guide(),
              CanApplyOptimization(
                  _, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  auto fragment_nav = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/page1#section"), main_rfh());
  fragment_nav->CommitSameDocument();
  Mock::VerifyAndClearExpectations(mock_optimization_guide());

  // Show the inline cue on /page1#section.
  observer->OnTextSelectionChanged(nullptr, u"Shown Selection");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->show_selection_affordance_called());
  observer->Reset();
  observer->set_call_base_update_selection_state(true);

  // Same-document pushState navigation to /page2 DOES re-query and updates
  // eligibility for subsequent selections.
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url2, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/false)));

  auto push_state_nav =
      content::NavigationSimulator::CreateRendererInitiated(url2, main_rfh());
  push_state_nav->CommitSameDocument();

  observer->OnTextSelectionChanged(nullptr, u"Selection On Page 2");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(observer->show_selection_affordance_called());
}

TEST_F(GlicSelectionObserverQuickAnswersTest,
       IgnoresStaleCallbackAfterNavigationAndQueriesExistingUrlOnCreation) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->set_call_base_update_selection_state(true);

  const GURL url1("https://example.com/stale");
  const GURL url2("https://example.com/fresh");
  optimization_guide::OptimizationGuideDecisionCallback stale_callback;
  optimization_guide::OptimizationGuideDecisionCallback fresh_callback;

  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url1, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&stale_callback));
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url2, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&fresh_callback));

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  ASSERT_TRUE(stale_callback);
  ASSERT_TRUE(fresh_callback);

  // Fresh navigation resolves to eligible; stale navigation resolves to
  // ineligible afterwards and must be ignored.
  std::move(fresh_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/true));
  std::move(stale_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/false));

  observer->OnTextSelectionChanged(nullptr, u"Fresh Selection");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Fresh Selection", *observer->last_affordance_text());

  // Recreating the tool and observer on a WebContents that already has a
  // committed URL should register QUICK_ANSWERS and query CanApplyOptimization
  // immediately.
  EXPECT_CALL(*mock_optimization_guide(),
              RegisterOptimizationTypes(ElementsAre(
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS)))
      .Times(1);
  EXPECT_CALL(
      *mock_optimization_guide(),
      CanApplyOptimization(
          url2, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/false)));

  RecreateQuickAnswersToolAndObserver();
  GetObserver()->set_call_base_update_selection_state(true);

  GetObserver()->OnTextSelectionChanged(nullptr, u"Recreated Selection");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(GetObserver()->show_selection_affordance_called());

  observer_.reset();
}

}  // namespace glic
